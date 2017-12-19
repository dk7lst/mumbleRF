TARGET = libsbcelt.a

CFLAGS = -O2 -I../celt-0.7.0/libcelt/ -DSBCELT_PREFIX

SOURCES = \
	libsbcelt.c \
	mtime.c \
	stub.c

ifeq ($(shell uname -s),Linux)
SOURCES += \
	futex-linux.c \
	closefrom.c
endif

ifeq ($(shell uname -s),Darwin)
SOURCES += \
	futex-stub.c \
	closefrom.c
endif

ifeq ($(shell uname -s),FreeBSD)
SOURCES += \
	futex-freebsd.c \
	closefrom-sys.c
endif

include ../Make.lib
