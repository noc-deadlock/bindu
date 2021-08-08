#!/bin/bash
# script sweeps the number of bubbles for
# low load latency and saturation throughtput

# for bubbles in 1 2 3 4 8 12 16 32 64
topologies=( '64_nodes-connectivity_matrix_0-links_removed_0.txt' '64_nodes-connectivity_matrix_0-links_removed_1.txt' '64_nodes-connectivity_matrix_0-links_removed_4.txt' '64_nodes-connectivity_matrix_0-links_removed_8.txt' '64_nodes-connectivity_matrix_0-links_removed_12.txt' )
# for file in 0 1 2 3 4
for file in 0
do
	for bubbles in 1 2 3 4 8 12 16 32 64
	do
		python main_irregular.py ${topologies[$file]} ${bubbles} 1 1 &
	done
done
