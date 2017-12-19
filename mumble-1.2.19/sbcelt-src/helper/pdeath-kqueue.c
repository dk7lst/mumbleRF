// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "eintr.h"

static int kqfd = -1;

static void *pdeath_monitor(void *udata) {
	(void) udata;
	while (1) {
		struct kevent ke;
		int ret = HANDLE_EINTR(kevent(kqfd, NULL, 0, &ke, 1, NULL));
		if (ret == 0) {
			// No events for us.
			continue;
		} else if (ret > 0) {
			// Our parent has died.
			_exit(0);
		} else if (ret == -1) {
			// Most of kevent()'s returned errors are programmer errors,
			// except ENOMEM and ESRCH.
			//
			// In case of ENOMEM, we might as well exit.  It's not clear
			// when ESRCH is returned, but the racy case of the parent
			// going away before we set up our kevent struct is handled
			// by the pdeath function below, so it should be safe to ignore.
			if (errno == ENOMEM) {
				_exit(50);
			}
			return NULL;
		}
	}
	return NULL;
}

int pdeath() {
	struct kevent ke;
	pid_t ppid;

	kqfd = kqueue();
	if (kqfd == -1) {
		return -1;
	}

	ppid = getppid();
	if (ppid <= 1) {
		_exit(0);
	}

	EV_SET(&ke, ppid, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
	if (HANDLE_EINTR(kevent(kqfd, &ke, 1, NULL, 0, NULL)) == -1) {
		// Parent seemingly already dead.
		if (errno == ESRCH) {
			_exit(0);
		}
		return -1;
	}

	pthread_t thr;
	if (pthread_create(&thr, NULL, pdeath_monitor, NULL) == -1) {
		return -1;
	}

	if (getppid() <= 1) {
		_exit(0);
	}

	return 0;
}
