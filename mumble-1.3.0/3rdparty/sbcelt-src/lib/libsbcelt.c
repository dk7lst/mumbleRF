// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>

#include "celt.h"
#include "../sbcelt-internal.h"
#include "../sbcelt.h"

#include "eintr.h"
#include "futex.h"
#include "mtime.h"
#include "debug.h"
#include "closefrom.h"

static struct SBCELTWorkPage *workpage = NULL;
static struct SBCELTDecoderPage *decpage = NULL;
static int running = 0;
static int lastslot = 0;
static uint64_t lastrun = 3000; // 3 ms
static int fdin = -1;
static int fdout = -1;
static pid_t hpid = -1;
static uint64_t lastdead = 0;
static pthread_t monitor;

int SBCELT_FUNC(celt_decode_float_rw)(CELTDecoder *st, const unsigned char *data, int len, float *pcm);
int SBCELT_FUNC(celt_decode_float_futex)(CELTDecoder *st, const unsigned char *data, int len, float *pcm);
int SBCELT_FUNC(celt_decode_float_picker)(CELTDecoder *st, const unsigned char *data, int len, float *pcm);
int (*SBCELT_FUNC(celt_decode_float))(CELTDecoder *, const unsigned char *, int, float *) = SBCELT_FUNC(celt_decode_float_picker);

// SBELT_HelperBinary returns the path to the helper binary.
static char *SBCELT_HelperBinary() {
	char *helper = getenv("SBCELT_HELPER_BINARY");
	if (helper == NULL) {
		helper = "/usr/bin/sbcelt-helper";
	}
	return helper;
}

// SBCELT_CheckSeccomp checks for kernel support for
// SECCOMP.
//
// On success, the function returns a valid sandbox
// mode (see SBCELT_SANDBOX_*).
//
// On failure, the function returns
//  -1 if the helper process did not execute correctly.
//  -2 if the fork system call failed. This signals that the
//     host system is running low on memory. This is a
//     recoverable error, and in our case we should simply
//     wait a bit and try again.
static int SBCELT_CheckSeccomp() {
 	int status, err;
	pid_t child;

	child = fork();
	if (child == -1) {
 		return -2;
	} else if (child == 0) {
		char *const argv[] = {
			SBCELT_HelperBinary(),
			"detect",
			NULL,
		};
		execv(argv[0], argv);
		_exit(100);
	}

	if (HANDLE_EINTR(waitpid(child, &status, 0)) == -1) {
		return -1;
	}

	if (!WIFEXITED(status)) {
		return -1;
	}

	int code = WEXITSTATUS(status);
	if (!SBCELT_SANDBOX_VALID(code)) {
		return -1;
	}

	return code;
}

// SBCELT_HelperMonitor implements a monitor thread that runs
// when libsbcelt decides to use SBCELT_MODE_FUTEX.  It is
// response for determining whether the helper process has died,
// and if that happens, restart it.
static void *SBCELT_HelperMonitor(void *udata) {
	(void) udata;

	while (1) {
		uint64_t now = mtime();
		uint64_t elapsed = now - lastdead;
		lastdead = now;

		// Throttle helper re-launches to around 1 per sec.
		if (elapsed < 1*USEC_PER_SEC) {
			usleep(1*USEC_PER_SEC);
		}

		debugf("restarted sbcelt-helper; %lu usec since last death", elapsed);

		pid_t child = fork();
		if (child == -1) {
			// We're memory constrained. Wait and try again...
			usleep(5*USEC_PER_SEC);
			continue;
		} else if (child == 0) {
			// For SBCELT_SANDBOX_SECCOMP_BPF, it shouldn't matter
			// whether the child inherits any file descriptors, since
			// the only useful system call the process can make is futex(2).
			//
			// However, if we're running in Futex mode without a sandbox,
			// closing the file descriptors is indeed a good idea, which
			// is why we do it unconditionally below.
			(void) HANDLE_EINTR(close(0));
			(void) HANDLE_EINTR(close(1));
#ifndef DEBUG
			xclosefrom(2);
#else
			xclosefrom(3);
#endif

			char *const argv[] = {
				SBCELT_HelperBinary(),
				NULL,
			};
			execv(argv[0], argv);
			_exit(100);
		}

		int status;
		int retval = HANDLE_EINTR(waitpid(child, &status, 0));
		if (retval == child) {
			if (WIFEXITED(status)) {
				debugf("sbcelt-helper died with exit status: %i", WEXITSTATUS(status));
			} else if (WIFSIGNALED(status)) {
				debugf("sbcelt-helper died with signal: %i", WTERMSIG(status));
			}
		} else if (retval == -1 && errno == EINVAL) {
			fprintf(stderr, "libsbcelt: waitpid() failed with EINVAL; internal error!\n");
			fflush(stderr);
			exit(1);
		}
	}
}

// SBCELT_RelaunchHelper restarts the helper process in a state which
// makes it usable for SBCELT_MODE_RW.
//
// On success, returns 0.
// On failure, returns
//   -1 if the system is resource exhauted (not enough memory for fork(2),
//      no available file descriptor slots for pipe(2), or because of
//      throttling of helper process launches).
static int SBCELT_RelaunchHelper() {
	int fds[2];
	int chin, chout;

	// Best-effort attempt to reap the previous helper process.
	// Perhaps the process libsbcelt is hosted in is ignoring
	// SIGCHLD, in which case the kernel will automatically reap
	// the children, making this call superfluous.
	int reap = (hpid != -1);
	if (reap) {
		int status;
		int retval = HANDLE_EINTR(waitpid(hpid, &status, WNOHANG));
		if (retval == hpid) {
			if (WIFEXITED(status)) {
				debugf("sbcelt-helper died with exit status: %i", WEXITSTATUS(status));
			} else if (WIFSIGNALED(status)) {
				debugf("sbcelt-helper died with signal: %i", WTERMSIG(status));
			}
		} else if (retval == -1 && errno == EINVAL) {
			fprintf(stderr, "libsbcelt: waitpid() failed with EINVAL; internal error!\n");
			fflush(stderr);
			exit(1);
		}
		hpid = -1;
	}

	// Throttle helper re-launches to around 1 per sec.
	uint64_t now = mtime();
	uint64_t elapsed = now - lastdead;
	if (reap) {
		lastdead = now;
	}
	if (elapsed < 1*USEC_PER_SEC) {
		return -1;
	}

	if (pipe(fds) == -1)
		return -1;
	fdin = fds[0];
	chout = fds[1];

	if (pipe(fds) == -1) {
		(void) HANDLE_EINTR(close(fdin));
		(void) HANDLE_EINTR(close(chout));
		return -1;
	}
	fdout = fds[1];
	chin = fds[0];

	debugf("SBCELT_RelaunchHelper; fdin=%i, fdout=%i", fdin, fdout);

	pid_t child = fork();
	if (child == -1) {
		(void) HANDLE_EINTR(close(fdin));
		(void) HANDLE_EINTR(close(chout));
		(void) HANDLE_EINTR(close(fdout));
		(void) HANDLE_EINTR(close(chin));
		return -1;
	} else if (child == 0) {
		(void) HANDLE_EINTR(close(0));
		(void) HANDLE_EINTR(close(1));
		if (HANDLE_EINTR(dup2(chin, 0)) == -1)
			_exit(101);
		if (HANDLE_EINTR(dup2(chout, 1)) == -1)
			_exit(102);

		// Make sure that all file descriptors, except
		// those strictly needed by the helper are closed.
		// Allow the child to inherit stderr for debugging
		// purposes only.
#ifndef DEBUG
		xclosefrom(2);
#else
		xclosefrom(3);
#endif

		char *const argv[] = {
			SBCELT_HelperBinary(),
			NULL,
		};
		execv(argv[0], argv);
		_exit(100);
	}

	(void) HANDLE_EINTR(close(chin));
	(void) HANDLE_EINTR(close(chout));

	hpid = child;
	debugf("SBCELT_RelaunchHelper; relaunched helper (pid=%li)", child);

	return 0;
}

int SBCELT_Init() {
	char shmfn[50];
	if (snprintf(&shmfn[0], 50, "/sbcelt-%lu", (unsigned long) getpid()) < 0) {
		return -1;
	}

	shm_unlink(&shmfn[0]);
	int fd = shm_open(&shmfn[0], O_CREAT|O_RDWR, 0600);
	if (fd == -1) {
		return -1;
	} else {
		if (ftruncate(fd, SBCELT_PAGES*SBCELT_PAGE_SIZE) == -1) {
			debugf("unable to truncate: %s (%i)", strerror(errno), errno);
			return -1;
		}

		void *addr = mmap(NULL, SBCELT_PAGES*SBCELT_PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, fd, 0);
		if (addr == MAP_FAILED) {
			debugf("unable to mmap: %s (%i)", strerror(errno), errno);
			return -1;
		}
		memset(addr, 0, SBCELT_PAGES*SBCELT_PAGE_SIZE);

		workpage = addr;
		decpage = addr+SBCELT_PAGE_SIZE;

		workpage->busywait = sysconf(_SC_NPROCESSORS_ONLN) > 0;

		int i;
		for (i = 0; i < SBCELT_SLOTS; i++) {
			decpage->slots[i].available = 1;
		}
	}

	return 0;
}

CELTDecoder *SBCELT_FUNC(celt_decoder_create)(const CELTMode *mode, int channels, int *error) {
	if (!running) {
		SBCELT_Init();
		running = 1;
	}

	// Find a free slot.
	int i, slot = -1;
	for (i = 0; i < SBCELT_SLOTS; i++) {
		int idx = (lastslot+i) % SBCELT_SLOTS;
		if (decpage->slots[idx].available) {
			decpage->slots[idx].available = 0;
			lastslot = idx;
			slot = idx;
			break;
		}
	}
	if (slot == -1) {
		debugf("decoder_create: no free slots");
		return NULL;
	}

	debugf("decoder_create: slot=%i", slot);

	return (CELTDecoder *)((uintptr_t)slot);
}

void SBCELT_FUNC(celt_decoder_destroy)(CELTDecoder *st) {
	int slot = (int)((uintptr_t)st);
	decpage->slots[slot].available = 1;

	debugf("decoder_destroy: slot=%i", slot);
}

// celt_decode_float_picker is the initial value for the celt_decode_float function pointer.
// It checks the available sandbox modes on the system, picks an appropriate celt_decode_float
// implementation to use according to the available sandbox modes, and makes those choices
// available to the helper process in the work page.
int SBCELT_FUNC(celt_decode_float_picker)(CELTDecoder *st, const unsigned char *data, int len, float *pcm) {
	int sandbox = SBCELT_CheckSeccomp();
	if (sandbox == -1) {
		return CELT_INTERNAL_ERROR;
 	// If the system is memory constrained, pretend that we were able
	// decode a frame correctly, and delegate the seccomp availability
	// check to sometime in the future.
	} else if (sandbox == -2) {
		memset(pcm, 0, sizeof(float)*480);
		return CELT_OK;
	}

	// For benchmarking and testing purposes, it's beneficial for us
	// to force a SECCOMP_STRICT sandbox, and therefore be force to run
	// in the rw mode.
	if (getenv("SBCELT_PREFER_SECCOMP_STRICT") != NULL) {
		if (sandbox == SBCELT_SANDBOX_SECCOMP_BPF) {
			sandbox = SBCELT_SANDBOX_SECCOMP_STRICT;
		}
	}

	debugf("picker: chose sandbox=%i", sandbox);
	workpage->sandbox = sandbox;

	// If we're without a sandbox, or is able to use seccomp
	// with BPF filters, we can use our fast futex mode.
	// For seccomp strict mode, we're limited to rw mode.
	switch (workpage->sandbox) {
		case SBCELT_SANDBOX_SEATBELT:
			workpage->mode = SBCELT_MODE_RW;
			SBCELT_FUNC(celt_decode_float) = SBCELT_FUNC(celt_decode_float_rw);
			break;
		case SBCELT_SANDBOX_SECCOMP_STRICT:
			workpage->mode = SBCELT_MODE_RW;
			SBCELT_FUNC(celt_decode_float) = SBCELT_FUNC(celt_decode_float_rw);
			break;
		default:
			if (futex_available()) {
				workpage->mode = SBCELT_MODE_FUTEX;
				SBCELT_FUNC(celt_decode_float) = SBCELT_FUNC(celt_decode_float_futex);
			} else {
				workpage->mode = SBCELT_MODE_RW;
				SBCELT_FUNC(celt_decode_float) = SBCELT_FUNC(celt_decode_float_rw);
			}
			break;
	}

	debugf("picker: chose mode=%i", workpage->mode);

	if (workpage->mode == SBCELT_MODE_RW) {
#ifndef SBCELT_NO_SIGNAL_MUCKING
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = SIG_IGN;
		sigemptyset(&sa.sa_mask);
		if (sigaction(SIGPIPE, &sa, NULL) == -1) {
			return CELT_INTERNAL_ERROR;
		}
#endif
	} else if (workpage->mode == SBCELT_MODE_FUTEX) {
		pthread_t tmp;
		if (pthread_create(&monitor, NULL, SBCELT_HelperMonitor, NULL) != 0) {
			return CELT_INTERNAL_ERROR;
		}
	}

	return SBCELT_FUNC(celt_decode_float)(st, data, len, pcm);
}

int SBCELT_FUNC(celt_decode_float_futex)(CELTDecoder *st, const unsigned char *data, int len, float *pcm) {
	int slot = (int)((uintptr_t)st);

	debugf("decode_float; len=%i", len);

	workpage->slot = slot;
	memcpy(&workpage->encbuf[0], data, len);
	workpage->len = len;

	uint64_t begin = mtime();

	// Wake up the helper, if necessary...
	workpage->ready = 0;
	futex_wake(&workpage->ready);

	int bad = 0;
	if (workpage->busywait) {
		while (!workpage->ready) {
			uint64_t elapsed = mtime() - begin;
			if (elapsed > lastrun*2) {
				bad = 1;
				break;
			}
		}
	} else {
		do {
			struct timespec ts = { 0, (lastrun*2) * NSEC_PER_USEC };
			int err = futex_wait(&workpage->ready, 0, &ts);
			if (err == 0) {
				break;
			} else if (err == ETIMEDOUT) {
				bad = 1;
				break;
			}
		} while (!workpage->ready);
	}

	if (!bad) {
		uint64_t elapsed = mtime() - begin;
#ifdef DYNAMIC_TIMEOUT
		lastrun = elapsed;
#endif
		debugf("spent %lu usecs in decode\n", elapsed);
		memcpy(pcm, workpage->decbuf, sizeof(float)*480);
	} else {
#ifdef DYNAMIC_TIMEOUT
		lastrun = 3000;
#endif
		memset(pcm, 0, sizeof(float)*480);
	}

	return CELT_OK;
}

int SBCELT_FUNC(celt_decode_float_rw)(CELTDecoder *st, const unsigned char *data, int len, float *pcm) {
	int slot = (int)((uintptr_t)st);
	ssize_t remain;
	void *dst;

	debugf("decode_float; len=%i", len);

	workpage->slot = slot;
	memcpy(&workpage->encbuf[0], data, len);
	workpage->len = len;

	// First time the library user attempts to decode
	// a frame, schedule a restart.  After that, restarts
	// should only happen when the helper dies.
	int restart = (hpid == -1);
	int attempts = 0;
retry:
	// Limit ourselves to two attempts before giving the
	// caller an empty PCM frame in its buffer.  We'll
	// try again next time celt_decode_float() is called.
	++attempts;
	if (attempts > 2) {
		debugf("decode_float; too many failed attempts, returning empty frame");
		memset(pcm, 0, sizeof(float)*480);
		return CELT_OK;
	}

	if (restart) {
		(void) HANDLE_EINTR(close(fdout));
		(void) HANDLE_EINTR(close(fdin));
		if (SBCELT_RelaunchHelper() == -1) {
			goto retry;
		}
		restart = 0;
	}

	// Signal to the helper that it should begin to work.
	if (HANDLE_EINTR(write(fdout, &workpage->pingpong, 1)) == -1) {
		// Only attempt to restart the helper process
		// if our write failed with EPIPE. That's the
		// only indication we have that our helper has
		// died.
		//
		// For other errno's, simply use another attempt.
		if (errno == EPIPE) {
			restart = 1;
		}
		goto retry;
	}

	// Wait for the helper to signal us that it has decoded
	// the frame we passed to it.
	if (HANDLE_EINTR(read(fdin, &workpage->pingpong, 1)) == -1) {
		// Restart helper in case of EPIPE. See comment
		// inside the error condition for the write(2)
		// call above.
		if (errno == EPIPE) {
			restart = 1;
		}
		goto retry;
	}

	debugf("decode_float; success");

	memcpy(pcm, workpage->decbuf, sizeof(float)*480);

	return CELT_OK;
}

