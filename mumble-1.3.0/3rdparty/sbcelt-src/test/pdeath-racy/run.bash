#!/usr/bin/env bash

UNAME=$(uname -s | tr [:upper:] [:lower:])
case "${UNAME}" in
	"linux") KIND="linux" ;;
	"freebsd") KIND="kqueue" ;;
	"darwin") KIND="kqueue" ;;
	*) exit 2 ;;
esac

rm -f _test
cc  -I ../../helper -I ../../lib ../../helper/pdeath-${KIND}.c pdeath_racy.c -lpthread -o _test
childpid=$(./_test)
sleep 1
/bin/kill -0 ${childpid} 2>/dev/null

if [ $? -eq 0 ]
then
	/bin/kill -9 ${childpid} 2>/dev/null
	exit 1
else
	exit 0
fi
