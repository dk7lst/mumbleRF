#!/usr/bin/env bash

rm -f _test
cc  ../../helper/alloc.c alloc_ctor.c -o _test
./_test
if [ $? -eq 0 ]
then
	exit 0
else
	exit 1
fi
