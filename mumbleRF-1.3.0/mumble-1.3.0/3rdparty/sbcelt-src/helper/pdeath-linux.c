// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

int pdeath() {
	if (prctl(PR_SET_PDEATHSIG, SIGKILL) == -1)
		return -1;
	if (getppid() <= 1)
		_exit(0);

	return 0;
}
