// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#ifndef __SBCELT_INTERNAL_H__
#define __SBCELT_INTERNAL_H__

#define SBCELT_PAGES                   2
#define SBCELT_PAGE_SIZE               4096

#define SBCELT_SLOTS                   40

#define SBCELT_MODE_FUTEX              1
#define SBCELT_MODE_RW                 2

#define SBCELT_SANDBOX_NONE            0
#define SBCELT_SANDBOX_SECCOMP_STRICT  1
#define SBCELT_SANDBOX_SECCOMP_BPF     2
#define SBCELT_SANDBOX_SEATBELT        3
#define SBCELT_SANDBOX_CAPSICUM        4

#define SBCELT_SANDBOX_VALID(x) \
	(x >= SBCELT_SANDBOX_NONE && x <= SBCELT_SANDBOX_CAPSICUM)

struct SBCELTWorkPage {
	int            slot;          // The slot that the helper process should work on.
	int            ready;         // The ready state (also used for futex synchronization).
	unsigned char  busywait;      // Determines whether libsbcelt and the helper may use busy-waiting instead of kernel synchronization.
	unsigned char  mode;          // The current operational mode (SBCELT_MODE_FUTEX or SBCELT_MODE_RW)
	unsigned char  sandbox;       // The sandbox technique that is used to jail the helper.
	unsigned char  pingpong;      // Byte-sized value used for SBCELT_MODE_RW synchronization.
	unsigned int   len;           // Specifies the length of encbuf to the helper process.
	unsigned char  encbuf[2048];  // Holds the frame to be decoded.
	float          decbuf[480];   // Holds the decoded PCM data.
};

struct SBCELTDecoderSlot {
	int   available;
	int   dispose;
};

struct SBCELTDecoderPage {
	struct SBCELTDecoderSlot slots[SBCELT_SLOTS];
};

int SBCELT_Init();

#endif
