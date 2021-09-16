#!bin/bash

make


./IMR setting/sequential-inplace.txt test/systor-traces-sample.csv test/systor-traces-sample-sequential-inplace.trace
./IMR setting/sequential-outplace.txt test/systor-traces-sample.csv test/systor-traces-sample-sequential-outplace.trace

./IMR setting/crosstrack-inplace.txt test/systor-traces-sample.csv test/systor-traces-sample-crosstrack-inplace.trace
./IMR setting/crosstrack-outplace.txt test/systor-traces-sample.csv test/systor-traces-sample-crosstrack-outplace.trace

./IMR setting/partition.txt test/systor-traces-sample.csv test/systor-traces-sample-partition.trace