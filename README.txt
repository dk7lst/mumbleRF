=== Build-Anleitung ===
https://wiki.mumble.info/wiki/BuildingLinux#For_Debian_.2F_Ubuntu

=== Vorbereitungen ===
Unix-User in die Gruppen "audio" (für ALSA-Support), "sudo" (für
WiringPi-Installer) und "gpio" (für GPIO ohne root-Rechte) aufnehmen.

=== Pakete installieren ===
-> install-dep.sh als root ausführen.

=== Bauen ===
-> build.sh als normaler User ausführen.

=== Konfigurieren ===
Mumble-Config (unter ~/.config/Mumble/Mumble.conf) um eine Sektion
[extptt]
erweitern.

Wichtig: Die Bezeichnungen PTT/SQL sind auf Mumble bezogen definiert.
D.h. wenn PTT gesetzt wird, sendet Mumble, nicht das angeschlossene
Funkgerät!

Darin sind folgende Parameter möglich:

mode=<x> (Default: 0)
PTT-Modus:
0: GPIO-Modus mit User-Rechten. Die entsprechenden Pins müssen zuvor
   richtig initialisiert worden sein. Der Mumble-User muss ggf. Mitglied
   der Gruppe "gpio" sein.
1: GPIO-Modus mit Root-Rechten.
2: RS232-Modus. Der Mumble-User muss ggf. Mitglied der Gruppe "dialout"
   sein.

pttpin=<x> (Default: 17)
GPIO-Eingangs-Pin (nach BCM-Zählweise), über das Mumble auf Sendung
geschaltet wird. (nur für 0<=mode<=1 relevant)

sqlpin=<x> (Default: 27)
GPIO-Ausgangs-Pin (nach BCM-Zählweise), das Mumble setzt wenn Audio
von anderen Teilnehmern empfangen wird. (nur für 0<=mode<=1 relevant)

serialdevice=<x> (Default: /dev/ttyUSB0)
Serieller-Port, dessen CTS/RTS-Leitungen für PTT/SQL-Tastung genutzt
werden sollen, (z.B. /dev/ttyUSB0). (nur für mode=2 relevant)

invertptt=<true|false> (Default: true)
invertsql=<true|false> (Default: false)
Signal des PTT- bzw. SQL-Pins invertieren.

=== Starten ===
Wenn ohne root-Rechte gearbeitet werden soll (was der Sicherheit wegen zu
empfehlen ist!) vor dem Start von Mumble "configure-pins.sh" als root
starten damit die GPIOs richtig konfiguriert werden.

Dann "run.sh" als normaler User ausführen.
