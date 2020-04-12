# Copyright 2005-2019 The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

# winpaths_static.pri
#
# This file contains the default paths for Windows dependencies
# when running in the win32-static build environment from the
# mumble-releng repo.
#
# This file is included by winpaths_default.pri if the 'static'
# config option is passed to qmake.
#
# If you want to customize them do NOT CHANGE THIS FILE but instead
# create a winpaths_custom.pri file in this directory and overwrite
# the variables you need to customize. Do NOT COMMIT your custom file
# to the repository.
#

ICE_PATH = $$(MUMBLE_PREFIX)/ZeroC-Ice
BZIP2_PATH = $$(MUMBLE_PREFIX)/bzip2
PROTOBUF_PATH = $$(MUMBLE_PREFIX)/protobuf
OPENSSL_PATH = $$(MUMBLE_PREFIX)/OpenSSL
LIBSNDFILE_PATH = $$(MUMBLE_PREFIX)/sndfile
BOOST_PATH = $$(MUMBLE_PREFIX)/Boost
BONJOUR_PATH = $$(MUMBLE_PREFIX)/bonjour
ASIO_PATH = $$(MUMBLE_PREFIX)/asio
G15SDK_PATH = $$(MUMBLE_PREFIX)/G15SDK/LCDSDK
