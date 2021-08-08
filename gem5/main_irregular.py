# Hello World program in Python
import os
import sys
import subprocess

bench_caps=[ "UNIFORM_RANDOM", "BIT_COMPLEMENT", "BIT_REVERSE", "BIT_ROTATION", "TRANSPOSE", "SHUFFLE" ]
# bench_caps=[ "TRANSPOSE" ]
bench=[ "uniform_random", "bit_complement", "bit_reverse", "bit_rotation", "transpose", "shuffle" ]
# bench=[ "transpose" ]
routing_algorithm=[ "Adaptive_Random" ]
num_cores=64
file=sys.argv[1]
bubble=int(sys.argv[2])
intraPeriod=int(sys.argv[3])
interPeriod=int(sys.argv[4])
d="05-09-2019"
out_dir="/usr/scratch/mayank/brownianNetwork_nocs2019_rslt/"+d
cycles=100000
vnet=0

VC=[ 2, 4 ]
for b in range(len(bench)):
	for vc_ in VC:
		print ("b: {0:s} vc-{1:d}".format(bench_caps[b], vc_))
		pkt_lat = 0
		injection_rate = 0.02
		while(pkt_lat < 100.00 and injection_rate < 0.60 ):

			os.system("./build/Garnet_standalone/gem5.opt -d {0:s}/{1:d}c/Torus/{4:s}/{3:s}/{2:s}/vc-{5:d}/bubble-{6:d}/intra-{7:d}_inter-{8:d}/inj-{9:1.2f} configs/example/garnet_synth_traffic.py --topology=Torus --num-cpus=64 --num-dirs=64 --mesh-rows=8 --network=garnet2.0 --router-latency=1 --sim-cycle={10:d} --conf-file={3:s} --sim-type=1 --enable-bn=1 --rand-bb=1 --num-bubble={6:d} --intra-period={7:d} --inter-period={8:d} --inj-vnet=0 --vcs-per-vnet={5:d} --injectionrate={9:1.2f} --synthetic={11:s} --routing-algorithm=0".format(out_dir, num_cores, bench_caps[b], file, routing_algorithm[0], vc_, bubble, intraPeriod, interPeriod, injection_rate, cycles, bench[b]))

			inj_rate="{:1.2f}".format(injection_rate)
			output_dir=out_dir+"/"+str(num_cores)+"c/Torus/"+ routing_algorithm[0]+"/"+file+"/"+bench_caps[b]+"/vc-"+str(vc_)+"/bubble-"+str(bubble)+"/intra-"+str(intraPeriod)+"_inter-"+str(interPeriod)+"/inj-"+inj_rate

			print ("output_dir: %s" %(output_dir))
			# print("grep -nri average_flit_latency {0:s} | sed 's/.*system.ruby.network.average_flit_latency\s*//'".format(output_dir))
			packet_latency = subprocess.check_output("grep -nri average_flit_latency  {0:s}  | sed 's/.*system.ruby.network.average_flit_latency\s*//'".format(output_dir), shell=True)
			print packet_latency
			pkt_lat = float(packet_latency)
			# print ("Packet Latency: %f"%((float)packet_latency))
			print ("Packet Latency: %f" %(pkt_lat))
			injection_rate+=0.02
			print ("injection_rate: %1.2f" %(injection_rate))
