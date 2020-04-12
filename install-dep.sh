#!/bin/sh
# Install packets, see "https://wiki.mumble.info/wiki/BuildingLinux#For_Debian_.2F_Ubuntu":
apt-get update
apt-get install build-essential pkg-config qt5-default qttools5-dev-tools libqt5svg5-dev \
                libboost-dev libasound2-dev libssl-dev \
                libspeechd-dev libzeroc-ice-dev libpulse-dev \
                libcap-dev libprotobuf-dev protobuf-compiler \
                libogg-dev libavahi-compat-libdnssd-dev libsndfile1-dev \
                libxi-dev
