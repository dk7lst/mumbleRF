// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#ifndef __FUTEX_H__
#define __FUTEX_H__

#define FUTEX_TIMEDOUT    -2
#define FUTEX_INTERRUPTED -3

int futex_available();
int futex_wake(int *futex);
int futex_wait(int *futex, int val, struct timespec *ts);

#endif
