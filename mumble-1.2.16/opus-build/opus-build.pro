include(../compiler.pri)

BUILDDIR=$$basename(PWD)
SOURCEDIR=$$replace(BUILDDIR,-build,-src)

!win32 {
	VERSION=0 #$$ Fool packaging script
}

!exists(../$$SOURCEDIR/COPYING) {
	message("The $$SOURCEDIR/ directory was not found. You need to do one of the following:")
	message("")
	message("git submodule init")
	message("git submodule update")
	message("")
	error("Aborting configuration")
}

TEMPLATE = lib
CONFIG -= qt
CONFIG += debug_and_release
#CONFIG -= warn_on
#CONFIG += warn_off
VPATH	= ../$$SOURCEDIR
TARGET = opus
DEFINES += HAVE_CONFIG_H OPUS_BUILD

QMAKE_CFLAGS -= -fPIE -pie

win32 {
  DEFINES += WIN32 _WIN32
  INCLUDEPATH += ../$$BUILDDIR/win32

  CONFIG(sse2) {
    TARGET_VERSION_EXT = .sse2
  } else {
    QMAKE_CFLAGS_RELEASE -= -arch:SSE
    QMAKE_CFLAGS_DEBUG -= -arch:SSE
  }
}

unix {
  CONFIG += staticlib
  QMAKE_CFLAGS += -x c++
  INCLUDEPATH += ../$$BUILDDIR
}

DIST = config.h

INCLUDEPATH *= \
../$$SOURCEDIR/celt \
../$$SOURCEDIR/include \
../$$SOURCEDIR/silk \
../$$SOURCEDIR/silk/float

SOURCES *= \
celt/bands.c \
celt/celt.c \
celt/celt_lpc.c \
celt/cwrs.c \
celt/entcode.c \
celt/entdec.c \
celt/entenc.c \
celt/kiss_fft.c \
celt/laplace.c \
celt/mathops.c \
celt/mdct.c \
celt/modes.c \
celt/pitch.c \
celt/quant_bands.c \
celt/rate.c \
celt/vq.c

SOURCES *= \
silk/float/apply_sine_window_FLP.c \
silk/float/autocorrelation_FLP.c \
silk/float/burg_modified_FLP.c \
silk/float/bwexpander_FLP.c \
silk/float/corrMatrix_FLP.c \
silk/float/encode_frame_FLP.c \
silk/float/energy_FLP.c \
silk/float/find_LPC_FLP.c \
silk/float/find_LTP_FLP.c \
silk/float/find_pitch_lags_FLP.c \
silk/float/find_pred_coefs_FLP.c \
silk/float/inner_product_FLP.c \
silk/float/k2a_FLP.c \
silk/float/levinsondurbin_FLP.c \
silk/float/LPC_analysis_filter_FLP.c \
silk/float/LPC_inv_pred_gain_FLP.c \
silk/float/LTP_analysis_filter_FLP.c \
silk/float/LTP_scale_ctrl_FLP.c \
silk/float/noise_shape_analysis_FLP.c \
silk/float/pitch_analysis_core_FLP.c \
silk/float/prefilter_FLP.c \
silk/float/process_gains_FLP.c \
silk/float/regularize_correlations_FLP.c \
silk/float/residual_energy_FLP.c \
silk/float/scale_copy_vector_FLP.c \
silk/float/scale_vector_FLP.c \
silk/float/schur_FLP.c \
silk/float/solve_LS_FLP.c \
silk/float/sort_FLP.c \
silk/float/warped_autocorrelation_FLP.c \
silk/float/wrappers_FLP.c \
silk/A2NLSF.c \
silk/ana_filt_bank_1.c \
silk/biquad_alt.c \
silk/bwexpander.c \
silk/bwexpander_32.c \
silk/check_control_input.c \
silk/CNG.c \
silk/code_signs.c \
silk/control_audio_bandwidth.c \
silk/control_codec.c \
silk/control_SNR.c \
silk/dec_API.c \
silk/decode_core.c \
silk/decode_frame.c \
silk/decode_indices.c \
silk/decode_parameters.c \
silk/decode_pitch.c \
silk/decode_pulses.c \
silk/decoder_set_fs.c \
silk/enc_API.c \
silk/encode_indices.c \
silk/encode_pulses.c \
silk/gain_quant.c \
silk/HP_variable_cutoff.c \
silk/init_decoder.c \
silk/init_encoder.c \
silk/inner_prod_aligned.c \
silk/interpolate.c \
silk/lin2log.c \
silk/log2lin.c \
silk/LP_variable_cutoff.c \
silk/LPC_analysis_filter.c \
silk/LPC_inv_pred_gain.c \
silk/NLSF2A.c \
silk/NLSF_decode.c \
silk/NLSF_del_dec_quant.c \
silk/NLSF_encode.c \
silk/NLSF_stabilize.c \
silk/NLSF_unpack.c \
silk/NLSF_VQ.c \
silk/NLSF_VQ_weights_laroia.c \
silk/NSQ.c \
silk/NSQ_del_dec.c \
silk/pitch_est_tables.c \
silk/PLC.c \
silk/process_NLSFs.c \
silk/quant_LTP_gains.c \
silk/resampler.c \
silk/resampler_down2.c \
silk/resampler_down2_3.c \
silk/resampler_private_AR2.c \
silk/resampler_private_down_FIR.c \
silk/resampler_private_IIR_FIR.c \
silk/resampler_private_up2_HQ.c \
silk/resampler_rom.c \
silk/shell_coder.c \
silk/sigm_Q15.c \
silk/sort.c \
silk/stereo_decode_pred.c \
silk/stereo_encode_pred.c \
silk/stereo_find_predictor.c \
silk/stereo_LR_to_MS.c \
silk/stereo_MS_to_LR.c \
silk/stereo_quant_pred.c \
silk/sum_sqr_shift.c \
silk/table_LSF_cos.c \
silk/tables_gain.c \
silk/tables_LTP.c \
silk/tables_NLSF_CB_NB_MB.c \
silk/tables_NLSF_CB_WB.c \
silk/tables_other.c \
silk/tables_pitch_lag.c \
silk/tables_pulses_per_block.c \
silk/VAD.c \
silk/VQ_WMat_EC.c

SOURCES *= \
src/opus.c \
src/opus_decoder.c \
src/opus_encoder.c \
src/repacketizer.c

CONFIG(debug, debug|release) {
	CONFIG += console
	DESTDIR = ../debug
}

CONFIG(release, debug|release) {
	DESTDIR = ../release
}

macx:!CONFIG(static) {
	libname.target = libname
	libname.commands = cd ${DESTDIR} && install_name_tool -id `pwd`/${TARGET} ${TARGET}
	libname.depends = ${DESTDIR}${TARGET}
	libname.CONFIG = recursive
	QMAKE_EXTRA_TARGETS *= libname
	ALL_DEPS += libname
}

include(../symbols.pri)
