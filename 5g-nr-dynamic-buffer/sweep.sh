#!/bin/bash
# static FIFO sweep: 20/83/200 pkts x seeds 1-3, 300 s, sequential, frozen binary
cd ~/ns3/ns-3.48
export LD_LIBRARY_PATH=$PWD/build/lib
for n in 20 83 200; do for s in 1 2 3; do
  build/scratch/mixed-traffic/ns3.48-mixed-traffic-optimized --q=fifo --n=$n --seed=$s --simTime=300 >> ~/sweep.log 2>&1
done; done
echo DONE >> ~/sweep.log
