#!/bin/bash
for k in 1 4 8 16 32 64 128 256 512
# for k in 1 64 1024 2048 4096 8192 16384 32768 65536
# for k in 1 8 32 64
# for k in 200 300 400 500 600 700 800 900 1000
# for k in 1 10 20 30 40 50 60 70 80 90 100
do
./run_script64c_irregular.sh 64_nodes-connectivity_matrix_0-links_removed_1.txt 1 ${k} ${k} &
./run_script64c_irregular.sh 64_nodes-connectivity_matrix_0-links_removed_1.txt 1 1 ${k} &
./run_script64c_irregular.sh 64_nodes-connectivity_matrix_0-links_removed_12.txt 1 ${k} ${k} &
./run_script64c_irregular.sh 64_nodes-connectivity_matrix_0-links_removed_12.txt 1 1 ${k} &
# ./run_script64c_irregular.sh 64_nodes-connectivity_matrix_0-links_removed_8.txt 1 1 ${k} &
# ./run_script64c_irregular.sh 64_nodes-connectivity_matrix_0-links_removed_12.txt 1 1 ${k} &
done
