#!/bin/bash

RESULTS_DIR=results_same_lba

mkdir -p $RESULTS_DIR

for test_cycle_num in 1000; do
    for copy_count in 30; do
        # Run a loop for 10 times
        for i in {1..10}
        do
            # Run ./vscode-copy-test and change first argument from 1, 2, 4 to 32
            for bs in 1 2 4 8 16 32; do
                echo "$test_cycle_num $copy_count $i $bs"
                taskset 0x1 ../.build/examples/vscode-copy-test $bs $test_cycle_num $copy_count > $RESULTS_DIR/iter_${test_cycle_num}_each${copy_count}_bs_${bs}_run_${i}.txt 2>&1
            done
        done
    done
done