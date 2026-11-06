# Dynamic FIFO Buffer Management for Low-Latency, Fair 5G NR Mixed Traffic

Delay-driven dynamic buffer sizing (**Dyn**) and fair-share drop (**FSD**) for a plain FIFO queue, evaluated on 5G NR mixed traffic in ns-3 + 5G-LENA. Reproduces the six-AQM study of Naik et al. (COMSNETS 2025) and compares it with static FIFO sizes.

## Idea

A 1 GB buffer at a 10 Mbps bottleneck holds about 800 s of queue, so a plain FIFO has very high delay. No fixed size fixes it: 20 packets cuts throughput, 83 (the BDP) costs 42 ms.

- **Dyn**: every 100 ms read the max sojourn time. After 3 periods above 25 ms halve the limit (min 20). When below 12.5 ms add 5 (max 200). Start at 83.
- **FSD**: above the limit, drop an arrival only if its flow already holds `floor(limit / activeFlows)` packets, or the queue reaches 2 x limit. O(1) per packet, no per-flow queues, no scheduler.

Details: [docs/design.md](docs/design.md).

## Results

Mean of 3 seeds, 300 s, 10 Mbps / 40 ms bottleneck, 7 flows (UDP burst, TCP bulk, FTP, HTTP, VoIP, gaming, UDP video).

| Config | Mean delay (ms) | Throughput (Mbps) | J2 fairness (UDP ON) |
|---|---|---|---|
| PFIFO, 1 GB | 946.8 | 9.37 | 0.556 |
| FIFO-83 (BDP) | 42.0 | 8.87 | 0.505 |
| FIFO-20 | 9.5 | 6.97 | 0.504 |
| Dyn | 15.0 | 8.15 | 0.507 |
| **Dyn + FSD** | **9.6** | **8.17** | **0.524** |
| FQ-PIE (best AQM) | 9.7 | 8.85 | 0.971 |

- Dyn + FSD matches FIFO-20 delay with 17% more throughput, and cuts delay 77% against FIFO-83 for 8% less throughput.
- FSD lifts TCP bulk goodput about 4x during UDP bursts, but a single FIFO cannot match flow-queuing isolation (FQ-PIE).

Full tables, validation against the paper and known gaps: [RESULTS.md](RESULTS.md).

## Layout

```
5g-nr-dynamic-buffer/
  scratch/mixed-traffic/   mixed-traffic.cc, fsd-queue-disc.{h,cc}
  run.sh sweep.sh dynruns.sh fsdruns.sh   experiment drivers
  plot.sh fig2.gp fig3.gp fig4.gp         gnuplot figures
  results/ results-udp9/                  summaries, time series, figures
docs/                                     design, ns-3 upstream plan
RESULTS.md  BUILD.md
```

## Quick start

Needs ns-3.48 with 5G-LENA (`nr`, branch `5g-lena-v5.0.y`) and about 3 GB RAM. Full steps in [BUILD.md](BUILD.md).

```bash
cp -r 5g-nr-dynamic-buffer/scratch/mixed-traffic <ns-3.48>/scratch/
cd <ns-3.48> && ./ns3 build -j2
./ns3 run --no-build "mixed-traffic --q=fsd --seed=1 --simTime=60"
```

`--q` selects `pfifo | codel | pie | cobalt | fqcodel | fqpie | fifo | dyn | fsd`.

## Limitations

- DASH traffic is omitted (not in ns-3 mainline).
- The paper does not state AQM parameters or the UDP burst rate; defaults and 10 Mbps are assumed. PFIFO delay is sensitive to that rate.
- One scenario, 3 seeds. Validation against the paper is by trend and ranking.
- Delay is qdisc sojourn time only.

## Roadmap

Move Dyn inside the queue disc and propose `FsdQueueDisc` upstream to ns-3: [docs/patch-notes.md](docs/patch-notes.md).

## Reference

Naik et al., "Study of 5G NR Network Against Mixed Traffic", COMSNETS 2025. DOI 10.1109/COMSNETS63942.2025.10885598.

## Author

P Sai Jishnu

Developed as a course mini-project.

## License

GPL-2.0-only, see [LICENSE](LICENSE). Copyright (C) 2026 P Sai Jishnu.