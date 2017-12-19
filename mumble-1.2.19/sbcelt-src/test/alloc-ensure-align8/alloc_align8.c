#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
	size_t i;
	for (i = 0; i < 200; i++) {
		uintptr_t addr = (uintptr_t) malloc(i);
		if ((addr % 8) != 0) {
			fprintf(stderr, "allocation of size %lu not 8-byte aligned: %p\n", (unsigned long)i, (void *)addr);
			return 1;
		}
	}
	return 0;
}
