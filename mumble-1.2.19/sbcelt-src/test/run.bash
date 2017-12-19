#!/usr/bin/env bash

RUN_TESTS=$(cat RUN | grep -v "^#")
declare -i OK=0
declare -i FAIL=0
declare -i SKIP=0

for dir in ${RUN_TESTS}; do
	cd ${dir}
	echo -n "${dir}: "
	./run.bash
	ret=$?
	if [ $ret -eq 0 ]; then
		echo 'ok'
		OK=OK+1
	fi
	if [ $ret -eq 1 ]; then
		echo 'fail'
		FAIL=FAIL+1
	fi
	if [ $ret -eq 2 ]; then
		echo 'skip'
		SKIP=SKIP+1
	fi
	cd ..
done

echo ""
echo "${OK} ok, ${FAIL} failed, ${SKIP} skipped"
