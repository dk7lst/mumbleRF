#!/bin/sh
# Get modules from .gitmodules when 'git submodules' isn't working:
rm -f speex/.keep celt-0.7.0-src/.keep celt-0.11.0-src/.keep opus-src/.keep sbcelt-src/.keep 3rdparty/mach-override-src/.keep
git clone https://git.xiph.org/speex.git speex
git clone https://git.xiph.org/celt.git celt-0.7.0-src
git clone https://git.xiph.org/celt.git celt-0.11.0-src
git clone https://git.xiph.org/opus.git opus-src
git clone https://github.com/mumble-voip/sbcelt.git sbcelt-src
git clone https://github.com/mumble-voip/mach_override.git 3rdparty/mach-override-src
