#!/usr/bin/env bash

rm -rf results
mkdir -p results

export SBCELT_HELPER_BINARY=`pwd`/../helper/sbcelt-helper

cp ../bench-celt/test.dat .

# celt lib
for i in {0..9}
do
	../bench-celt/celt-bench > results/celt.${i}
done

# sbcelt lib (futex mode)
for i in {0..9}
do
	../bench-sbcelt/sbcelt-bench > results/sbcelt-futex.${i}
done

# sbcelt lib (rw mode)
export SBCELT_PREFER_SECCOMP_STRICT=1
for i in {0..9}
do
	../bench-sbcelt/sbcelt-bench > results/sbcelt-rw.${i}
done
unset SBCELT_PREFER_SECCOMP_STRICT

./crunch.py
