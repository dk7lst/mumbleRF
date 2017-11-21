# Mumble Overlay scripting addition
# (for injection into running processes)

include(../../compiler.pri)

CONFIG += x86_64 x86 debug_and_release
CONFIG(universal) {
	CONFIG += ppc
}

TEMPLATE = lib
CONFIG += plugin plugin_bundle
CONFIG -= gui qt

CONFIG(static) {
	CONFIG -= static
}

TARGET = MumbleOverlay
QMAKE_INFO_PLIST = osax.plist

QMAKE_LFLAGS = -framework Foundation -framework Cocoa
QMAKE_BUNDLE_EXTENSION = .osax
QMAKE_LFLAGS_PLUGIN = -bundle

SDEF.files = MumbleOverlay.sdef
SDEF.path = Contents/Resources
QMAKE_BUNDLE_DATA += SDEF

OBJECTIVE_SOURCES = osax.m
DIST = osax.plist MumbleOverlay.sdef

CONFIG(debug, debug|release) {
  DESTDIR       = ../../debug
}

CONFIG(release, debug|release) {
  DESTDIR       = ../../release
}

include(../../symbols.pri)
