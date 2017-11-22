#!/bin/sh
# Pakete installieren gemäß "https://wiki.mumble.info/wiki/BuildingLinux#For_Debian_.2F_Ubuntu":
# Zusätzlich für WiringPi-Lib auch git installieren.
apt-get update
apt-get install build-essential pkg-config qt4-dev-tools libqt4-dev libspeex1 \
                libspeex-dev libboost-dev libasound2-dev libssl-dev g++ \
                libspeechd-dev libzeroc-ice-dev libpulse-dev \
                libcap-dev libspeexdsp-dev libprotobuf-dev protobuf-compiler \
                libogg-dev libavahi-compat-libdnssd-dev libsndfile1-dev \
                libg15daemon-client-dev libxi-dev git-core libopus-dev \
                libcelt-dev
