#!bin/bash

make


./IMR setting/sequential-inplace.txt test/systor-traces-sample.csv test/systor-traces-sample-sequential-inplace.trace test/systor-traces-sample-sequential-inplace.eval
./IMR setting/sequential-outplace.txt test/systor-traces-sample.csv test/systor-traces-sample-sequential-outplace.trace test/systor-traces-sample-sequential-outplace.eval

./IMR setting/crosstrack-inplace.txt test/systor-traces-sample.csv test/systor-traces-sample-crosstrack-inplace.trace test/systor-traces-sample-crosstrack-inplace.eval
./IMR setting/crosstrack-outplace.txt test/systor-traces-sample.csv test/systor-traces-sample-crosstrack-outplace.trace test/systor-traces-sample-crosstrack-outplace.eval

./IMR setting/partition.txt test/systor-traces-sample.csv test/systor-traces-sample-partition.trace test/systor-traces-sample-partition.eval