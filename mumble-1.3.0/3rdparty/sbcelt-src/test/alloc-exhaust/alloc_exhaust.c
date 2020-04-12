#include <stdlib.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
	size_t i;

	// Try to allocate 10 chunks of 1MB each.
	// The current allocator is limited to 8MB per launch,
	// and kills itself if its allocation arena is exhausted.
	for (i = 0; i < 10; i++) {
		size_t chunk = 1024*1024; // 1MB
		uintptr_t val = (uintptr_t) malloc(chunk);
		(void) val;
	}

	return 0;
}
