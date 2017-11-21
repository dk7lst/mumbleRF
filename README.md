# mumbleRF
A patched version of the Mumble voice chat software for use with two-way radio transceivers (ham radio, CB, PMR and others).

This software allows to connect two-way-radio transceivers to Mumble/murmur servers in order to extend the range of radio-frequency communications.

# Features
The software provides:
- A squelch (SQL) output to be connected with the transceiver's PTT (push to talk) input in order to set the transceiver to transmit-mode when audio is received from the mumble server.
- A PTT-input driven by the transceiver's SQL-output for sending audio to the mumble server when a radio frequency call is received.

The software is optimized for Raspberry Pi single board computers. Other platforms may be supported in the future.

Digital I/O is available via RS232 (USB2Serial or native) and Raspberry Pi GPIO-ports.

# Branches and versioning
Branches are named according to the underlying Mumble release version.

(Select a branch from the github-menu!)

# Authors
Mumble source code: https://www.mumble.info/

Main development of the patches: Lars Struß, DK7LST (http://www.dk7lst.de/)

# Licensing and legal stuff
The software is intended for educational purpose in the context of amateur radio, citizen band and other hobbyist radio services.

The code (original code and patches) is subject to the Mumble license as described in the "LICENSE"-file of the corresponding sub-directories.

The code and additional files are provided "as is" without any warranties. Use at your own risk!

Make sure your equipment is safe (e.g. electrical safety, proper grounding, lightning protection) and complies to the radio and frequency regulations of your country!

Live the ham spirit and share your knowledge for a better world!
