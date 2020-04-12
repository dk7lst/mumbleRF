RUN_TESTS=$(cat RUN | grep -v "^#")

for dir in ${RUN_TESTS}; do
	cd ${dir}
	./clean.bash
	cd ..
done
