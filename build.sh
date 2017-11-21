#!/bin/sh

# In Verzeichnis der aktuellen Version wechseln:
cd mumble-1.2.16

# Makefiles erzeugen.
# Die diversen Fehlermeldungen scheinen normal zu sein weil ein Teil
# der Dateien erst zur Laufzeit gebaut wird?
qmake -recursive main.pro CONFIG+=no-server CONFIG+=no-crash-report CONFIG+=no-update CONFIG+=no-bonjour "$@"

# Bauen:
make clean
make
