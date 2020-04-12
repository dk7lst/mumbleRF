// Copyright (C) 2012 The SBCELT Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE-file.

#ifndef __SBCELT_H__
#define __SBCELT_H__

#ifdef __cplusplus
extern "C" {
#endif

// Define SBCELT_PREFIX_API to prefix all exported
// symbols with 'sb'.
#ifdef SBCELT_PREFIX_API
# define SBCELT_FUNC(x) sb ## x
  // Define SBCELT_COMPAT_API to have the preprocessor
  // replace all occurrences of calls to vanilla CELT
  // functions with calls to their SBCELT 'sb'-prefixed
  // counter-parts.
# ifdef SBCELT_COMPAT_API
#  define  celt_mode_create      sbcelt_mode_create
#  define  celt_mode_destroy     sbcelt_mode_destroy
#  define  celt_mode_info        sbcelt_mode_info
#  define  celt_encoder_create   sbcelt_encoder_create
#  define  celt_encoder_destroy  sbcelt_encoder_destroy
#  define  celt_encode_float     sbcelt_encode_float
#  define  celt_encode           sbcelt_encode
#  define  celt_encoder_ctl      sbcelt_encoder_ctl
#  define  celt_decoder_create   sbcelt_decoder_create
#  define  celt_decoder_destroy  sbcelt_decoder_destroy
#  define  celt_decode_float     sbcelt_decode_float
#  define  celt_decode           sbcelt_decode
#  define  celt_decoder_ctl      sbcelt_decoder_ctl
#  define  celt_strerror         sbcelt_strerror
# endif
#else
# define SBCELT_FUNC(x) x
#endif

CELTMode *SBCELT_FUNC(celt_mode_create)(celt_int32 Fs, int frame_size, int *error);
void SBCELT_FUNC(celt_mode_destroy)(CELTMode *mode);
int SBCELT_FUNC(celt_mode_info)(const CELTMode *mode, int request, celt_int32 *value);
CELTEncoder *SBCELT_FUNC(celt_encoder_create)(const CELTMode *mode, int channels, int *error);
void SBCELT_FUNC(celt_encoder_destroy)(CELTEncoder *st);
int SBCELT_FUNC(celt_encode_float)(CELTEncoder *st, const float *pcm, float *optional_synthesis,
                        unsigned char *compressed, int nbCompressedBytes);
int SBCELT_FUNC(celt_encode)(CELTEncoder *st, const celt_int16 *pcm, celt_int16 *optional_synthesis,
                  unsigned char *compressed, int nbCompressedBytes);
int SBCELT_FUNC(celt_encoder_ctl)(CELTEncoder * st, int request, ...);
CELTDecoder *SBCELT_FUNC(celt_decoder_create)(const CELTMode *mode, int channels, int *error);
void SBCELT_FUNC(celt_decoder_destroy)(CELTDecoder *st);
extern int (*SBCELT_FUNC(celt_decode_float))(CELTDecoder *st, const unsigned char *data, int len, float *pcm);
int SBCELT_FUNC(celt_decode)(CELTDecoder *st, const unsigned char *data, int len, celt_int16 *pcm);
int SBCELT_FUNC(celt_decoder_ctl)(CELTDecoder * st, int request, ...);
const char *SBCELT_FUNC(celt_strerror)(int error);

#ifdef __cplusplus
}
#endif

#endif
