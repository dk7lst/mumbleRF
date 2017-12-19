#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

#include <celt.h>
#include "common.h"

int gentest(unsigned char *buf, int n) {
	float pcm[FRAME_SIZE];
	int i;

        for (i = 0; i < FRAME_SIZE; i++) {
                pcm[i] = sinf(1000.0f * M_PI_2 * i / SAMPLE_RATE);
        }

        CELTMode *m = celt_mode_create(SAMPLE_RATE, FRAME_SIZE, NULL);
        CELTEncoder *e = celt_encoder_create(m, 1, NULL);

        if (celt_encode_float(e, pcm, NULL, buf, n)  != n) {
		return -1;
        }

	return 0;
}
