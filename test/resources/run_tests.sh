#!/bin/bash

set +e 

EXIT_CODE=0
for file in test_*; do
	[[ ${file} == *.dSYM ]] && continue
	export VOLK_GENERIC=1
	valgrind -v --error-exitcode=1 -q --tool=memcheck --leak-check=yes --show-reachable=yes ./${file}
	CURRENT_EXIT_CODE=$?
	if [ ${CURRENT_EXIT_CODE} != 0 ]; then
		EXIT_CODE=${CURRENT_EXIT_CODE} 
	fi  
done

exit ${EXIT_CODE}
