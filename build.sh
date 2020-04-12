#!/bin/sh

# Change to directory of current Mumble version:
cd mumble-1.3.0

# Create Makefiles.
# The various error messages seem to be normal because
# some of the files are built at runtime?
#
# Options:
#   extptt-serial: External PTT-support using serial port handshake lines.
#   extptt-wiringpi: External PTT-support for Raspberry Pi GPIO-port using WiringPi-library.
#   rtpaudio: Virtual network soundcard using Real-time Transport Protocol (RTP).
qmake -recursive main.pro CONFIG+=no-server CONFIG+=no-crash-report CONFIG+=no-update CONFIG+=no-bonjour CONFIG+=no-g15 CONFIG+=extptt-serial "$@"

# Start build:
make clean
make
