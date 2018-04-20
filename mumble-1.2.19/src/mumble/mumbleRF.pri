# External PTT-support for Raspberry Pi GPIO-port using WiringPi-library:
extptt-wiringpi {
	CONFIG *= extptt
	LIBS *= -lwiringPi
	DEFINES *= USE_EXTPTT_WIRINGPI
}

# External PTT-support using serial port handshake lines:
extptt-serial {
	CONFIG *= extptt
	DEFINES *= USE_EXTPTT_SERIAL
}

# General stuff common to all External PTT-modes:
extptt {
	DEFINES *= USE_EXTPTT
	HEADERS *= ExternalPTT.h
	SOURCES *= ExternalPTT.cpp
}
