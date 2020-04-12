// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define ALIGN_UP(addr,sz)    (((addr)+sz-1) & ~(sz-1))
#define ARENA_SIZE           8*1024*1024 // 8MB
#define ARENA_ALIGN          4096        // 4KB

static unsigned char arena[ARENA_SIZE] __attribute__((aligned (ARENA_ALIGN)));
static void *ptr = NULL;
static void *outside = NULL;

__attribute__((constructor))
static void malloc_ctor() {
	ptr = &arena[0];
	outside = ptr + ARENA_SIZE;
}

void *malloc(size_t size) {
	// CTOR sanity checking.
	if (ptr == NULL || outside == NULL) {
		_exit(51);
	}

	// Ensure our allocations end on a 8-byte boundary.
	size = size + sizeof(size_t);
	size = ALIGN_UP(size, 8);

	// Atomic add; returns old value.
	void *incr = (void *) size;
	void *region = __sync_fetch_and_add(&ptr, incr);
	// Check whether the region we were handed by the
	// allocator can actually fit in the arena.
	if ((region+size-1) >= outside) {
		_exit(50);
	// Also check the pointer itself exceeds the arena.
	} else if (region >= outside) {
		_exit(50);
	}

	size_t *sz = (size_t *) region;
	*sz = size;

	void *ret = region + sizeof(size_t);
	memset(ret, 0, size);

	return ret;
}

void *calloc(size_t nmemb, size_t size) {
	return malloc(nmemb*size);
}

void *realloc(void *ptr, size_t size) {
	size_t *oldsz = ptr-sizeof(size_t);
	void *dst = malloc(size);
	memcpy(dst, ptr, *oldsz);
	return dst;
}

void free(void *ptr) {
}
