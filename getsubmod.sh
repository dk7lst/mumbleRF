#!/bin/sh
cd mumble-1.2.19

# Get modules from .gitmodules when 'git submodules' isn't working:
rm -f speex/.keep celt-0.7.0-src/.keep celt-0.11.0-src/.keep opus-src/.keep sbcelt-src/.keep 3rdparty/mach-override-src/.keep
git clone https://git.xiph.org/speex.git speex
git clone https://git.xiph.org/celt.git celt-0.7.0-src
git clone https://git.xiph.org/celt.git celt-0.11.0-src
git clone https://git.xiph.org/opus.git opus-src
git clone https://github.com/mumble-voip/sbcelt.git sbcelt-src
git clone https://github.com/mumble-voip/mach_override.git 3rdparty/mach-override-src

# Switch to fixed versions needed:
cd speex
git checkout -q a6d05eb
cd ../celt-0.7.0-src
git checkout -q 6c79a93
cd ../celt-0.11.0-src
git checkout -q e3d39fe
cd ../opus-src
git checkout -q d060dd7
cd ../sbcelt-src
git checkout -q 045493d
cd ../3rdparty/mach-override-src
git checkout -q 919148f
cd ../..
