// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#include "sbcelt-sandbox.h"
#include "sbcelt-internal.h"
#include "seccomp-sandbox.h"

// Attempt to enter the specified sandbox mode.
int SBCELT_EnterSandbox(int mode) {
	switch (mode) {
		case SBCELT_SANDBOX_NONE:
			return 0;
		case SBCELT_SANDBOX_SECCOMP_STRICT:
			return seccomp_sandbox_strict_init();
		case SBCELT_SANDBOX_SECCOMP_BPF:
			return seccomp_sandbox_filter_init();
	}
	return -1;
}
