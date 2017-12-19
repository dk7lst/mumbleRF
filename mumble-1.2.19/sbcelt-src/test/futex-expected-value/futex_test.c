#include <stdio.h>
#include <sys/time.h>
#include "futex.h"

int main(int argc, char *argv[]) {
	int ftx = 0;
	int err = futex_wait(&ftx, 1, NULL);
	if (err != 0) {
		fprintf(stderr, "expected 0 return value, got %i\n", err);
		return 1;
	}
	return 0;
}
