include(../compiler.pri)

TEMPLATE	=app
CONFIG  += qt warn_on release console
QT -= gui
TARGET = SpeexMark
SOURCES = SpeexMark.cpp
HEADERS = Timer.h
INCLUDEPATH = ../src ../speex/include
LIBS += -lspeex
LIBPATH += ../release
