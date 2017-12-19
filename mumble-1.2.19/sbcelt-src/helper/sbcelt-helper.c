// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#include <sys/time.h>
#include <sys/syscall.h>

#include "celt.h"
#include "../sbcelt-internal.h"

#include "pdeath.h"
#include "eintr.h"
#include "futex.h"
#include "sbcelt-sandbox.h"

#define SAMPLE_RATE 48000

#ifdef DEBUG
# define debugf(fmt, ...) \
	do { \
		fprintf(stderr, "sbcelt-helper:%s():%u: " fmt "\n", \
			__FUNCTION__, __LINE__, ## __VA_ARGS__); \
		fflush(stderr); \
	} while (0)
#else
 #define debugf(s, ...) do{} while (0)
#endif

static struct SBCELTWorkPage *workpage = NULL;
static struct SBCELTDecoderPage *decpage = NULL;
static CELTMode *modes[SBCELT_SLOTS];
static CELTDecoder *decoders[SBCELT_SLOTS];

static void SBCELT_DecodeSingleFrame() {
	unsigned char *src = &workpage->encbuf[0];
	float *dst = &workpage->decbuf[0];

	int idx = workpage->slot;
	struct SBCELTDecoderSlot *slot = &decpage->slots[idx];
	CELTMode *m = modes[idx];
	CELTDecoder *d = decoders[idx];

	if (slot->dispose && m != NULL && d != NULL) {
		debugf("disposed of mode & decoder for slot=%i", idx);
		celt_mode_destroy(m);
		celt_decoder_destroy(d);
		m = modes[idx] = celt_mode_create(SAMPLE_RATE, SAMPLE_RATE / 100, NULL);
		d = decoders[idx] = celt_decoder_create(m, 1, NULL);
		slot->dispose = 0;
	}

	if (m == NULL && d == NULL) {
		debugf("created mode & decoder for slot=%i", idx);
		m = modes[idx] = celt_mode_create(SAMPLE_RATE, SAMPLE_RATE / 100, NULL);
		d = decoders[idx] = celt_decoder_create(m, 1, NULL);
	}

	debugf("got work for slot=%i", idx);
	unsigned int len = workpage->len;
	debugf("to decode: %p, %p, %u, %p", d, src, len, dst);
	if (len == 0) {
		celt_decode_float(d, NULL, 0, dst);
	} else {
		celt_decode_float(d, src, len, dst);
	}

	debugf("decoded len=%u", len);
}

static int SBCELT_FutexHelper() {
	while (1) {
		// Wait for the lib to signal us.
		while (workpage->ready == 1) {
			int err = futex_wait(&workpage->ready, 1, NULL);
			if (err == 0) {
				break;
			}
		}

		SBCELT_DecodeSingleFrame();

		workpage->ready = 1;

		if (!workpage->busywait) {
			futex_wake(&workpage->ready);
		}
	}

	return -2;
 }

static int SBCELT_RWHelper() {
	while (1) {
		// Wait for the lib to signal us.
		if (HANDLE_EINTR(read(0, &workpage->pingpong, 1)) == -1) {
			return -2;
		}

		SBCELT_DecodeSingleFrame();

		if (HANDLE_EINTR(write(1, &workpage->pingpong, 1)) == -1) {
			return -3;
		}
	}
}

int main(int argc, char *argv[]) {
	if (argc >= 2 && !strcmp(argv[1], "detect")) {
		debugf("in seccomp-detect mode");
		if (SBCELT_EnterSandbox(SBCELT_SANDBOX_CAPSICUM) == 0) {
			_exit(SBCELT_SANDBOX_CAPSICUM);
		}
		if (SBCELT_EnterSandbox(SBCELT_SANDBOX_SEATBELT) == 0) {
			_exit(SBCELT_SANDBOX_SEATBELT);
		}
		if (SBCELT_EnterSandbox(SBCELT_SANDBOX_SECCOMP_BPF) == 0) {
			_exit(SBCELT_SANDBOX_SECCOMP_BPF);
		}
		if (SBCELT_EnterSandbox(SBCELT_SANDBOX_SECCOMP_STRICT) == 0) {
			_exit(SBCELT_SANDBOX_SECCOMP_STRICT);
		}
		_exit(SBCELT_SANDBOX_NONE);
	}

	debugf("helper running");

	if (pdeath() == -1) {
		return 1;
	}

	char shmfn[50];
	if (snprintf(&shmfn[0], 50, "/sbcelt-%lu", (unsigned long) getppid()) < 0) {
		return 2;
	}

	int fd = shm_open(&shmfn[0], O_RDWR, 0600);
	if (fd == -1) {
		debugf("unable to open shm: %s (%i)", strerror(errno), errno);
		return 3;
	}

	void *addr = mmap(NULL, SBCELT_PAGES*SBCELT_PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		debugf("unable to mmap: %s (%i)", strerror(errno), errno);
		return 5;
	}
	workpage = addr;
	decpage = addr+SBCELT_PAGE_SIZE;

	debugf("workpage=%p, decpage=%p", workpage, decpage);

	if (SBCELT_EnterSandbox(workpage->sandbox) == -1) {
		return 6;
	}

	switch (workpage->mode) {
		case SBCELT_MODE_FUTEX:
			return SBCELT_FutexHelper();
		case SBCELT_MODE_RW:
			return SBCELT_RWHelper();
	}

	return 7;
}
