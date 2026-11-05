# Design

Two mechanisms on a plain droptail FIFO: **Dyn** resizes the buffer from measured delay, and **FSD** decides which packet to drop when the queue is full. Results are in [../RESULTS.md](../RESULTS.md).

## Problem

The base paper runs a 1 GB buffer at a 10 Mbps bottleneck: about 800 s of queue. A plain FIFO then has the worst delay (254 ms mean in the paper), and a UDP burst takes about 9 Mbps and starves everything else. The paper compares AQMs (CoDel, PIE, COBALT, FQ-CoDel, FQ-PIE) but never varies the buffer size. FQ-PIE is already the best AQM there, so improving CoDel treats a symptom. The oversized buffer is the cause.

No fixed size works for both cases:

| Size | Result |
|---|---|
| Small (20 packets) | Low delay, but throughput drops to 6.97 Mbps because bursts overflow |
| BDP (83 packets) | Full throughput, delay 42 ms |
| 200 packets | 82 ms, no throughput gain |

## Dyn: delay-driven AIMD on the FIFO limit

Every period T = 100 ms Dyn reads the maximum sojourn time m seen in that period.

```
if m > target (25 ms) for H consecutive periods:   limit = max(Lmin, limit / 2)
else if m < target / 2:                            limit = min(Lmax, limit + step)
apply: SetMaxSize(max(limit, occupancy))
```

| Parameter | Value |
|---|---|
| Period T | 100 ms |
| Target | 25 ms |
| Hold count H | 3 |
| Step | 5 packets |
| Limits Lmin / Lmax | 20 / 200 packets |
| Start | 83 packets (BDP: 10 Mbps x 100 ms RTT) |

Reasons:

- **Delay is the control signal.** On a fixed 10 Mbps link a rate x target rule is a constant, which is a static buffer. Measured delay reflects real congestion.
- **AIMD** shrinks fast under congestion and grows slowly when calm, like TCP.
- **Hold count H** keeps one short burst from halving the buffer. H = 1, 2 and 3 were tested (60 s, seed 1) and H = 3 gave the highest throughput.
- **Droptail FIFO only.** The scope is the plain FIFO that many devices still use.
- **Placement** is the router egress, before the PGW (plain IP, flows still visible).

Implementation: a `Simulator::Schedule` timer reads the max of the `SojournTime` trace over the last period and resizes the queue disc.

## FSD: fair-share drop

When the queue is at or above its soft limit, FSD drops the arriving packet only if its flow already holds its share. Light flows such as VoIP and gaming still get in.

```
enqueue(p):
  if queue >= limit:
      share = floor(limit / activeFlows)      # active flows include the arriving flow
      if flowCount[p.flow] >= share or queue >= 2 * limit:   # hard cap 400 packets
          drop(p); return
  push(p); flowCount[p.flow]++
dequeue: flowCount[p.flow]--
```

`FsdQueueDisc` subclasses `FifoQueueDisc` and keeps a per-flow packet count keyed by the flow hash. Per packet cost is O(1) and there is no scheduler, unlike FQ.

A longest-flow drop rule was tested as an ablation (`--longest`) and rejected: in one of three seeds TCP bulk was starved (0.008 Mbps).

## What FSD does and does not do

FSD raises TCP bulk goodput during UDP bursts about 4x (0.06 to 0.23 Mbps). It does not reach flow-queuing isolation (FQ-PIE gives 3.86 Mbps). A single FIFO cannot isolate an unresponsive UDP flow, so the fairness gain is partial.

## Experimental decisions

| ID | Decision |
|---|---|
| D2 | 300 s runs (the paper's "300 ms" in Table I is a typo) |
| D4 | Qdisc on the router egress, before the PGW. After the PGW packets are GTP-wrapped and flow hashing collapses to one flow |
| D5 | Very large qdisc MaxSize for the baselines, device TxQueue 1 packet |
| D6 | Default ns-3 AQM parameters (the paper gives none) |
| D7 | DASH omitted (not in ns-3 mainline), stated limitation |
| D8 | UDP burst 10 Mbps (the paper gives no rate) |
| D9 | Only `SojournTime`, `Drop` and sink `Rx` traces |
| D11 | Add Jain's index; J2 = Jain(UDP burst, TCP bulk) in UDP-ON windows is the meaningful fairness number |
| D13 | Validate against the paper by trend and ranking |
| D14 | 3 seeds per configuration |

## Compared configurations

PFIFO 1 GB, FIFO-20, FIFO-83, FIFO-200, Dyn, Dyn + FSD, and the six paper AQMs (CoDel, PIE, COBALT, FQ-CoDel, FQ-PIE, PFIFO Fast).

## Code layout

| File | Role |
|---|---|
| `scratch/mixed-traffic/mixed-traffic.cc` | topology, radio, seven applications, `--q` switch, Dyn controller `Ctl()`, trace collection, summary output |
| `scratch/mixed-traffic/fsd-queue-disc.h`, `.cc` | `FsdQueueDisc` |
| `run.sh`, `sweep.sh`, `dynruns.sh`, `fsdruns.sh` | experiment drivers |
| `plot.sh`, `fig2.gp`, `fig3.gp`, `fig4.gp` | gnuplot figures |
