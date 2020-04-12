#!/usr/bin/env bash

UNAME=$(uname -s | tr [:upper:] [:lower:])
case "${UNAME}" in
	"linux") ;;
	"freebsd") ;;
	*) exit 2 ;;
esac

rm -f _test
cc -I ../../lib futex_timeout.c ../../lib/futex-${UNAME}.c ../../lib/mtime.c -lpthread -o _test
./_test
if [ $? -eq 0 ]
then
	exit 0
else
	exit 1
fi
