// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#include <sys/time.h>

#include "futex.h"

int futex_available() {
	return 0;
}

int futex_wake(int *futex) {
	return -1;
}

int futex_wait(int *futex, int val, struct timespec *ts) {
	return -1;
}
