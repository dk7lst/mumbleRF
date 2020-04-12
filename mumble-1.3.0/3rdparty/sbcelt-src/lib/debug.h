// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef DEBUG
# define debugf(fmt, ...) \
        do { \
                fprintf(stderr, "libsbcelt:%s():%u: " fmt "\n", \
                    __FUNCTION__, __LINE__, ## __VA_ARGS__); \
                fflush(stderr); \
        } while (0)
#else
# define debugf(s, ...) do{} while (0)
#endif

#endif
