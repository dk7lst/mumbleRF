// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#include <sandbox.h>
#include <stdio.h>
#include <stdint.h>

#include "debug.h"
#include "sandbox.h"

#include "sbcelt-internal.h"

// Attempt to enter the specified sandbox mode.
int SBCELT_EnterSandbox(int mode) {
	switch (mode) {
		case SBCELT_SANDBOX_NONE:
			// No sandbox was requsted.
			return 0;
		case SBCELT_SANDBOX_SEATBELT: {
			char *errbuf = NULL;
			int retval = sandbox_init(kSBXProfilePureComputation, SANDBOX_NAMED, &errbuf);
			if (retval == -1) {
				debugf("unable to init seatbelt: %s", errbuf);
			}
			return retval;
		}
	
	}
	return -1;
}
