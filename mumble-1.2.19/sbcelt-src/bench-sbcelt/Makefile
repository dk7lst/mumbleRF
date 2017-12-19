include ../Make.conf

CFLAGS = -O2 -I../celt-0.7.0/libcelt/ -I..
LDFLAGS = ../lib/libsbcelt.a -lm -lpthread

ifeq ($(shell uname -s),Linux)
LDFLAGS += -lrt
endif

TARGET = sbcelt-bench

SOURCES = \
	bench.c

include ../Make.cmd
