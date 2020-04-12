// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#include <stdlib.h>

#include "celt.h"
#include "../sbcelt-internal.h"
#include "../sbcelt.h"

CELTMode *SBCELT_FUNC(celt_mode_create)(celt_int32 Fs, int frame_size, int *error) {
	return (CELTMode *) 0x1;
}

void SBCELT_FUNC(celt_mode_destroy)(CELTMode *mode) {
}

int SBCELT_FUNC(celt_mode_info)(const CELTMode *mode, int request, celt_int32 *value) {
	if (request == CELT_GET_BITSTREAM_VERSION) {
		*value = 0x8000000b;
		return CELT_OK;
	}
	return CELT_INTERNAL_ERROR;
}

CELTEncoder *SBCELT_FUNC(celt_encoder_create)(const CELTMode *mode, int channels, int *error) {
	return NULL;
}

void SBCELT_FUNC(celt_encoder_destroy)(CELTEncoder *st) {
}

int SBCELT_FUNC(celt_encode_float)(CELTEncoder *st, const float *pcm, float *optional_synthesis,
                      unsigned char *compressed, int nbCompressedBytes) {
	return CELT_INTERNAL_ERROR;
}

int SBCELT_FUNC(celt_encode)(CELTEncoder *st, const celt_int16 *pcm, celt_int16 *optional_synthesis,
                unsigned char *compressed, int nbCompressedBytes) {
	return CELT_INTERNAL_ERROR;
}

int SBCELT_FUNC(celt_encoder_ctl)(CELTEncoder * st, int request, ...) {
	return CELT_INTERNAL_ERROR;
}

int SBCELT_FUNC(celt_decode)(CELTDecoder *st, const unsigned char *data, int len, celt_int16 *pcm) {
	return CELT_INTERNAL_ERROR;
}

int SBCELT_FUNC(celt_decoder_ctl)(CELTDecoder * st, int request, ...) {
	return CELT_INTERNAL_ERROR;
}

const char *SBCELT_FUNC(celt_strerror)(int error) {
	return "celt: unknown error";
}
