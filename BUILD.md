# Build and run

Reproduces the experiments in [RESULTS.md](RESULTS.md) with ns-3.48 and 5G-LENA.

## Requirements

- Linux (developed on Lubuntu 26.04, x86-64)
- g++ 15.x (any C++20 compiler that builds ns-3.48), CMake 3.x or newer, Ninja, Python 3, Git, gnuplot 6
- About 3 GB RAM and 2 CPU cores minimum, 25 GB disk

## 1. Get ns-3.48 and 5G-LENA

```bash
mkdir -p ~/ns3 && cd ~/ns3
# ns-3.48 release tarball or git checkout, unpacked to ~/ns3/ns-3.48
cd ns-3.48
git clone -b 5g-lena-v5.0.y https://gitlab.com/cttc-lena/nr.git contrib/nr
```

The `nr` branch must match the ns-3 release. `5g-lena-v5.0.y` was used here with ns-3.48.

## 2. Configure and build

```bash
./ns3 configure -d optimized --enable-examples --disable-tests --enable-modules=nr
./ns3 build -j2
```

If the build stops with `cc1plus killed`, the machine ran out of memory: rebuild with `-j1`.

## 3. Add the project files

From the repository root:

```bash
cp -r 5g-nr-dynamic-buffer/scratch/mixed-traffic ~/ns3/ns-3.48/scratch/
cp 5g-nr-dynamic-buffer/*.sh 5g-nr-dynamic-buffer/*.gp ~/ns3/ns-3.48/
cd ~/ns3/ns-3.48 && ./ns3 build -j2
```

## 4. Run one simulation

```bash
./ns3 run --no-build "mixed-traffic --q=fqpie --seed=1 --simTime=60 --outDir=results/fqpie/s1"
```

| Option | Meaning |
|---|---|
| `--q` | `pfifo`, `codel`, `pie`, `cobalt`, `fqcodel`, `fqpie` (baseline AQMs, about 1 GB buffer), `fifo` (static limit `--n`), `dyn`, `fsd` (Dyn + FSD) |
| `--n` | static FIFO size in packets (with `--q=fifo`) |
| `--H` | Dyn hold count (default 3) |
| `--longest` | use the longest-flow drop rule (ablation) |
| `--udpRate` | UDP burst rate (default 10 Mbps) |
| `--seed` | RNG run number |
| `--simTime` | simulation time in seconds |
| `--outDir` | output directory |

Each run writes `goodput.dat`, `qdelay.dat`, `drops.dat` and, for `dyn`/`fsd`, `limit.dat` into its output directory, and appends one row to `results/summary.tsv`.

## 5. Run the full experiment set

| Script | Does |
|---|---|
| `run.sh [simTime] [seed]` | the six baseline AQMs, two at a time (defaults 60 s, seed 1) |
| `sweep.sh` | static FIFO sizes 20 / 83 / 200 |
| `dynruns.sh` | Dyn |
| `fsdruns.sh` | Dyn + FSD |
| `plot.sh` | 3-seed means to `results/avg/` (uses `fig2.gp`), trade-off and limit figures (`fig3.gp`), paper-style figures to `results/paper/` (`fig4.gp`) |

The published results use 300 s and seeds 1-3. Run at most two simulations in parallel on a 3 GB machine. Read the header of each script before running it to see the exact arguments it expects.

## Verify

A 60 s run of `--q=fqpie --seed=1` should show a steady queueing delay below about 20 ms and a throughput near 8-9 Mbps. For the 300 s numbers compare against `5g-nr-dynamic-buffer/results/summary.tsv`.

## Notes

- The qdisc is installed on the router egress before the PGW, so it sees plain IP packets. After the PGW packets are GTP-tunnelled and flow hashing would see one flow.
- DASH is not in ns-3 mainline and is not part of the traffic mix.
