#!/bin/bash

make all

MODES=("sequential-inplace" "sequential-outplace" "crosstrack-inplace" "crosstrack-outplace" "partition")

if [ "$1" == "systor17" ]; then
    TRACE=systor-traces-sample
    mkdir -p test/$TRACE
    for mode in ${MODES[*]}; do 
        mkdir -p test/$TRACE/$mode
        ./IMR $mode systor17 setting/sample.txt test/$TRACE.csv test/$TRACE/$mode/trace test/$TRACE/$mode/
    done
elif [ "$1" == "msr" ]; then 
    TRACE=msr-cambridge1-sample
    mkdir -p test/$TRACE
    for mode in ${MODES[*]}; do 
        mkdir -p test/$TRACE/$mode
        ./IMR $mode msr setting/sample.txt test/$TRACE.csv test/$TRACE/$mode/trace test/$TRACE/$mode/
    done
else
    echo expected 1 argument
fi

