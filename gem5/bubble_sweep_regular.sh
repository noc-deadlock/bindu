#!/bin/bash
# script sweeps the number of bubbles for 
# low load latency and saturation throughtput

for bubbles in 1 2 3 4 8 12 16 32 64
do
	./run_script64c.sh ${bubbles} 1 1 &
done
