#!/bin/bash
# waits for dynruns.sh, then FSD 3 seeds x 300 s
cd ~/ns3/ns-3.48
export LD_LIBRARY_PATH=$PWD/build/lib
while pgrep -f dynruns.sh >/dev/null; do sleep 20; done
for s in 1 2 3; do build/scratch/mixed-traffic/ns3.48-mixed-traffic-optimized --q=fsd --seed=$s --simTime=300 >> ~/fsd.log 2>&1; done
echo DONE >> ~/fsd.log
