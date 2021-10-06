#!bin/bash

make

MODES=("sequential-inplace" "sequential-outplace" "crosstrack-inplace" "crosstrack-outplace" "partition")
TRACE=systor-traces-sample

for mode in ${MODES[*]}; do 
    ./IMR setting/$mode.txt test/$TRACE.csv test/$TRACE-$mode.trace test/$TRACE-$mode.eval test/$TRACE-$mode.dist
done
