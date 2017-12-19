include ../Make.conf

CFLAGS = -O2 -DHAVE_CONFIG_H -I. -I../celt-0.7.0/libcelt/ -I../helper
LDFLAGS = -lm

CELT = ../celt-0.7.0/libcelt
TARGET = celt-bench

SOURCES = \
	$(CELT)/bands.c \
	$(CELT)/celt.c \
	$(CELT)/cwrs.c \
	$(CELT)/entcode.c \
	$(CELT)/entdec.c \
	$(CELT)/entenc.c \
	$(CELT)/header.c \
	$(CELT)/kiss_fft.c \
	$(CELT)/kiss_fftr.c \
	$(CELT)/laplace.c \
	$(CELT)/mdct.c \
	$(CELT)/modes.c \
	$(CELT)/pitch.c \
	$(CELT)/psy.c \
	$(CELT)/quant_bands.c \
	$(CELT)/rangedec.c \
	$(CELT)/rangeenc.c \
	$(CELT)/rate.c \
	$(CELT)/vq.c \
	gentest.c \
	bench.c

include ../Make.cmd
