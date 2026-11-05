#!/bin/bash
# usage: ./run.sh [simTime] [seed]   runs all 6 AQMs, 2 in parallel
T=${1:-60}; S=${2:-1}
cd ~/ns3/ns-3.48
printf '%s\n' pfifo codel pie cobalt fqcodel fqpie | \
  xargs -P2 -I{} ./ns3 run --no-build "mixed-traffic --q={} --seed=$S --simTime=$T"
