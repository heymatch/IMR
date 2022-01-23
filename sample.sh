#!/bin/bash

make all

MODES=("sequential-inplace" "sequential-outplace" "crosstrack-inplace" "crosstrack-outplace" "partition")

if [ "$1" == "systor17" ]; then
    TRACE=systor-traces-sample
    for mode in ${MODES[*]}; do 
        ./IMR $mode systor17 setting/sample.txt test/$TRACE.csv test/$TRACE-$mode.trace test/$TRACE-$mode
    done
elif [ "$1" == "msr" ]; then 
    TRACE=msr-cambridge1-sample
    for mode in ${MODES[*]}; do 
        ./IMR $mode msr setting/sample.txt test/$TRACE.csv test/$TRACE-$mode.trace test/$TRACE-$mode
    done
else
    echo expected 1 argument
fi

