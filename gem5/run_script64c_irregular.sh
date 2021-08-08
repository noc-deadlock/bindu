#!/bin/bash
#####	README
# .run_script.sh <num-cores> <routAlgo> <spin-freq> <rot> <uTurnSpinRing(3, 4)> <uTurnCrossbar(0, 1)>
###############################################################################
bench_caps=( 'UNIFORM_RANDOM' 'BIT_COMPLEMENT' 'BIT_REVERSE' 'BIT_ROTATION' 'TRANSPOSE' 'SHUFFLE' )
# bench_caps=( 'UNIFORM_RANDOM' )
bench=( 'uniform_random' 'bit_complement' 'bit_reverse' 'bit_rotation' 'transpose' 'shuffle' )
# bench=( 'uniform_random' )

routing_algorithm=( 'Adaptive_Random' )

d="05-30-2019"
conf_file=$1
bubble=$2
intraPeriod=$3
interPeriod=$4
out_dir="/usr/scratch/mayank/brownianNetwork_nocs2019_rslt/$d"
cycles=100000
vnet=0 #for multi-flit pkt: vnet = 2
tr=1
################# Give attention to the injection rate that you have got#############################
# for b in 0 1 2 3 4 5
for b in 0
do
for vc_ in 2
do
for k in 0.02 0.04 0.06 0.08 0.10 0.12 0.14 0.16 0.18 0.20 0.22 0.24 0.26 0.28 0.30 0.32 0.34
# for k in 0.12 0.14 0.16 0.18 0.20 0.22 0.24 0.26 0.28 0.32 0.34
do
	./build/Garnet_standalone/gem5.opt -d $out_dir/64c/Mesh-BN/${routing_algorithm[0]}/${conf_file}/${bench_caps[$b]}/vc-${vc_}/bubble-${bubble}/intra-${intraPeriod}_inter-${interPeriod}/inj-${k} configs/example/garnet_synth_traffic.py --topology=irregularMesh_XY --num-cpus=64 --num-dirs=64 --mesh-rows=8 --network=garnet2.0 --router-latency=1 --sim-cycles=${cycles} --inj-vnet=0 --vcs-per-vnet=${vc_} --conf-file=${conf_file} --sim-type=1 --enable-bn=1 --rand-bb=1 --num-bubble=${bubble} --intra-period=${intraPeriod} --inter-period=${interPeriod} --injectionrate=${k} --synthetic=${bench[$b]} --routing-algorithm=0 &
done
#sleep 300
done
done
