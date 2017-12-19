#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
	uintptr_t addr = (uintptr_t) malloc(10);
	// compensate for size_t
	addr -= sizeof(size_t);
	if ((addr % 4096) != 0) {
		fprintf(stderr, "expected 4k alignment for first allocation; got addr=%p\n", (void *)addr);
		return 1;
	}
	return 0;
}
