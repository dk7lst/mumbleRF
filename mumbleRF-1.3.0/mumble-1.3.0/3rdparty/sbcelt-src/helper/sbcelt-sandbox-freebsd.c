// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#include <sys/capability.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "debug.h"
#include "sbcelt-sandbox.h"

#include "sbcelt-internal.h"

// Attempt to enter the specified sandbox mode.
int SBCELT_EnterSandbox(int mode) {
	switch (mode) {
		case SBCELT_SANDBOX_NONE:
			// No sandbox was requsted.
			return 0;
		case SBCELT_SANDBOX_CAPSICUM: {
			int err = cap_enter();
			if (err == -1) {
				fprintf(stderr, "unable to cap_enter(): %s (%i)\n", strerror(errno), errno);
			}
			return err;
		}	
	}
	return -1;
}
