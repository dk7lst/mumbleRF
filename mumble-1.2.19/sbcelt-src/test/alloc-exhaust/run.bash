#!/usr/bin/env bash

rm -f _test
cc  ../../helper/alloc.c alloc_exhaust.c -o _test
./_test
if [ $? -eq 50 ]
then
	exit 0
else
	exit 1
fi
