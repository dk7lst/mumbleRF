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

(Select a branch from the github-menu! Otherwise you won't see the files!)

# Mumble Build Guide
https://wiki.mumble.info/wiki/BuildingLinux#For_Debian_.2F_Ubuntu

# Preparations
Run ./install-dep.sh as root to install many of Mumble's dependencies.

Use ./wiringpi-download.sh and ./wiringpi-build.sh to download and build
the WiringPi-library to use Raspberry Pi GPIO-ports.

Add unix user to the groups "audio" (for ALSA support), "sudo" (for
WiringPi installer) and "gpio" (for GPIO without root privileges).

# Build
-> Run ./build.sh as a normal (non-root) user.
See comments inside the file for advanced build options.

# Configure
Add a new section
[extptt]
to Mumble-Config (usually located at ~/.config/Mumble/Mumble.conf).

Important: The terms PTT/SQL are defined from Mumble's point of view.
That means: If PTT is set, Mumble will transmit, not the connected radio!

The following parameters are possible:

<pre>
mode=x (default: 0)
PTT mode:
0: GPIO mode with user (non-root) rights. The corresponding pins have to be
   properly initialized. The unix user Mumble is running on may need to be a
   member the group "gpio" (see /etc/group).
1: GPIO mode with root privileges.
2: RS232 mode with CRTSCTS-flag set. The Mumble user may need to be a
   member of the "dialout" group (see /etc/group).
3: RS232 mode with CRTSCTS-flag NOT set. The Mumble user may need to be a
   member of the "dialout" group (see /etc/group).

pttpin=x (default: 17)
GPIO input pin (according to BCM counting) which keys Mumble transmit.
(only relevant for 0 <= mode <= 1)

sqlpin=x (default: 27)
GPIO output pin (according to BCM counting) which Mumble sets when audio
is received from another Mumble subscriber.
(only relevant for 0 <= mode <= 1)

serialdevice=x (default: /dev/ttyUSB0)
Serial port device whose CTS/RTS lines will be used for PTT/SQL keying
(only relevant for mode = 2)

invertptt=true|false (default: true)
invertsql=true|false (default: false)
Invert the signal of the PTT or SQL pin.
</pre>

# Running the software
If you want to work without root privileges (highly recommend for
security reasons!) you may need to run "configure-pins.sh" as root prior
to running Mumble so that the GPIOs are configured correctly. Change the
script if you use other pins than the defaults mentioned above.

Then run "run.sh" as a normal user.

# Authors
Mumble source code: https://www.mumble.info/

Main development of the RF-patches: Lars Struß, DK7LST (http://www.dk7lst.de/)

# Licensing and legal stuff
The software is intended for educational purpose in the context of amateur radio, citizen band and other hobbyist radio services.

The code (original code and patches) is subject to the Mumble license as described in the "LICENSE"-file of the corresponding sub-directories.

The code and additional files are provided "as is" without any warranties. Use at your own risk!

Make sure your equipment is safe (e.g. electrical safety, proper grounding, lightning protection) and complies to the radio and frequency regulations of your country!

Live the ham spirit and share your knowledge for a better world!
