// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

// Based on bsd-closefrom.c by:
//
// Copyright (c) 2004-2005 Todd C. Miller <Todd.Miller@courtesan.com>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include <sys/types.h>
#include <dirent.h>

#include "eintr.h"

#if defined(__APPLE__)
# define FD_DIR "/dev/fd"
#else
# define FD_DIR "/proc/self/fd"
#endif

void xclosefrom(int lowfd) {
	struct dirent *dent;
	long fd, maxfd;
	char *endp;
	DIR *dirp;

	if ((dirp = opendir(FD_DIR))) {
		while ((dent = readdir(dirp)) != NULL) {
			fd = strtol(dent->d_name, &endp, 10);
			if (dent->d_name != endp && *endp == '\0' && fd >= 0 && fd < INT_MAX && fd >= lowfd && fd != dirfd(dirp)) {
				(void) HANDLE_EINTR(close((int)fd));
			}
		}
		(void) HANDLE_EINTR(closedir(dirp));
	} else {
		maxfd = sysconf(_SC_OPEN_MAX);
		for (fd = lowfd; fd < maxfd; fd++) {
			(void) HANDLE_EINTR(close((int)fd));
		}
	}
}
