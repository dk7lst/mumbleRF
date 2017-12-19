#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>

#include "futex.h"
#include "mtime.h"

static int ftx = 0;

void *waiter(void *udata) {
	(void) udata;
	usleep(USEC_PER_SEC*5);
}

int main(int argc, char *argv[]) {
	pthread_t thr;
	if (pthread_create(&thr, NULL, waiter, NULL) == -1) {
		return 1;
	}

	uint64_t begin = mtime();
	struct timespec ts = { 0, USEC_PER_SEC*2 };
	int err = futex_wait(&ftx, 0, &ts);
	if (err != FUTEX_TIMEDOUT) {
		fprintf(stderr, "expected ETIMEDOUT return value, got %i\n", err);
		return 1;
	}

	return 0;
}
