# Copyright (C) 2012 The SBCELT Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE-file.

include ../Make.conf

ORIGCFLAGS := $(CFLAGS)
CFLAGS = -Wall -Os $(ORIGCFLAGS)

ifeq ($(DEBUG),1)
CFLAGS += -DDEBUG
endif

ifeq ($(PREFIX),1)
CFLAGS += -DSBCELT_PREFIX_API
endif

ORIGLDFLAGS := $(LDFLAGS)
LDFLAGS = $(ORIGLDFLAGS)

ASMOBJS=$(SOURCES:.S=.S.o)
OBJECTS=$(ASMOBJS:.c=.c.o)

%.S.o: %.S
	@echo "ASM  $*.S"
	@$(CC) $(CFLAGS) -D__ASM__ -c $*.S -o $*.S.o

%.c.o: %.c
	@echo "CC   $*.c"
	@$(CC) $(CFLAGS) -c $*.c -o $*.c.o

$(TARGET): $(OBJECTS)
	@echo "LD   ${TARGET}"
	@$(LD) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $(TARGET)

clean:
	@rm -f $(TARGET) $(TARGET).list $(OBJECTS)
