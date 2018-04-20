#!/bin/sh

# Change to directory of current Mumble version:
cd mumble-1.2.19

# Create Makefiles.
# The various error messages seem to be normal because
# some of the files are built at runtime?
#
# Options:
#   extptt-serial: External PTT-support using serial port handshake lines.
#   extptt-wiringpi: External PTT-support for Raspberry Pi GPIO-port using WiringPi-library.
qmake -recursive main.pro CONFIG+=no-server CONFIG+=no-crash-report CONFIG+=no-update CONFIG+=no-bonjour CONFIG+=extptt-serial CONFIG+=extptt-wiringpi "$@"

# Start build:
make clean
make
