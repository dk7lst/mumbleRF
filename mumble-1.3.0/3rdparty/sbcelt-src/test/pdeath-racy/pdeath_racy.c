#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#include "pdeath.h"
#include "mtime.h"
#include "eintr.h"

int main(int argc, char *argv[]) {
	pid_t pid = fork();
	if (pid == 0) {
		// Close parent's fds.
		(void) HANDLE_EINTR(close(0));
		(void) HANDLE_EINTR(close(1));
		(void) HANDLE_EINTR(close(2));

		// Don't check return value.
		// If pdeath fails it's hard to signal to the parent,
		// and exitting will signal that pdeath *worked*.
		pdeath();
		while (1) {
			usleep(USEC_PER_SEC*1);
		}
	} else {
		printf("%lu\n", (unsigned long)pid);
	}
	return 0;
}
