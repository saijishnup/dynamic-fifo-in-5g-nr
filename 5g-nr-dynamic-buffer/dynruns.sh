#!/bin/bash
cd ~/ns3/ns-3.48
export LD_LIBRARY_PATH=$PWD/build/lib
for s in 1 2 3; do build/scratch/mixed-traffic/ns3.48-mixed-traffic-optimized --q=dyn --seed=$s --simTime=300 >> ~/dyn.log 2>&1; done
echo DONE >> ~/dyn.log
