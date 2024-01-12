#!/bin/bash

do_all_test() {
    for test_file in /home/lda/Projects/nesla/test/ProcessorTests/nes6502/v1/*.json; do
        ../build/test/cpu_test "$test_file"
        if [ $? -eq 1 ]; then
            exit 1
        fi 
    done
}

do_one_or_multiple_tests() {
    for name in $@; do
        ../build/test/cpu_test /home/lda/Projects/nesla/test/ProcessorTests/nes6502/v1/$name.json
        if [ $? -eq 1 ]; then
            exit 1
        fi 
    done
}

cd ../build/ && make
cd -
if [ $# -ge 1 ]; then
    do_one_or_multiple_tests $@
else
    do_all_test
fi
