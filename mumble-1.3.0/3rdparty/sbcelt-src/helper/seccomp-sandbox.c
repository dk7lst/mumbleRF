// seccomp example for x86 (32-bit and 64-bit) with BPF macros
//
// Copyright (c) 2012 The Chromium OS Authors <chromium-os-dev@chromium.org>
// Authors:
//  Will Drewry <wad@chromium.org>
//  Kees Cook <keescook@chromium.org>
//
// The code may be used by anyone for any purpose, and can serve as a
// starting point for developing applications using mode 2 seccomp.

#include "seccomp-sandbox.h"

int seccomp_sandbox_strict_init(void) {
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		return -1;
	}
	if (prctl(PR_SET_SECCOMP, 1, 0, 0, 0)) {
		return -1;
	}
	return 0;
}

int seccomp_sandbox_filter_init(void) {
	struct sock_filter filter[] = {
		/* Validate architecture. */
		VALIDATE_ARCHITECTURE,
		/* Grab the system call number. */
		EXAMINE_SYSCALL,
		/* List allowed syscalls. */
		ALLOW_SYSCALL(rt_sigreturn),
#ifdef __NR_sigreturn
		ALLOW_SYSCALL(sigreturn),
#endif
		ALLOW_SYSCALL(futex),
#ifdef DEBUG
		ALLOW_SYSCALL(write),
		ALLOW_SYSCALL(read),
		ALLOW_SYSCALL(fstat),
#endif
		ALLOW_SYSCALL(exit_group),
		ALLOW_SYSCALL(exit),
		KILL_PROCESS,
	};
	struct sock_fprog prog = {
		.len = (unsigned short)(sizeof(filter)/sizeof(filter[0])),
		.filter = filter,
	};

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		return -1;
	}
	if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
		return -1;
	}
	return 0;
}
