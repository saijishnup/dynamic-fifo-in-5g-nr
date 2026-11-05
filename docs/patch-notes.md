# Patch notes: moving Dyn and FSD into ns-3

Plan for an upstream contribution to ns-3 (`src/traffic-control`). Today the controller lives in the scratch program and the queue disc is standalone. This document lists what has to change. Nothing here is submitted yet.

## Current state (v0)

| Part | Where | How |
|---|---|---|
| Dyn controller | `mixed-traffic.cc`, `Ctl()` | A `Simulator::Schedule` timer every 100 ms reads the max `SojournTime` of the period and calls `SetMaxSize` (Dyn) or `SetLimit` (FSD) from outside the queue disc |
| FSD | `fsd-queue-disc.{h,cc}` | `FifoQueueDisc` subclass with a per-flow packet count (`m_flows`), drop rule at enqueue, counts lowered in `DoDequeue` |
| Parameters | constants in `mixed-traffic.cc` | T 100 ms, target 25 ms, H 3, step 5, limits 20 / 200, start 83 |

Problem for upstream: the controller is outside the queue disc, so users need the whole scratch program to use it.

## Target state

One self-contained queue disc, for example `FsdQueueDisc`, with the Dyn controller inside it.

- Dyn runs inside the queue disc: it records the sojourn time of each dequeued packet, keeps the period maximum, and a scheduled event evaluates the AIMD rule.
- All values become `TypeId` attributes with the current numbers as defaults.
- The queue disc keeps the FSD drop rule. Dyn can be switched off by an attribute so that FSD alone (fixed limit) is still possible.

| Attribute (proposed) | Default | Meaning |
|---|---|---|
| `Period` | 100 ms | controller period T |
| `Target` | 25 ms | delay target |
| `HoldPeriods` | 3 | consecutive bad periods before shrinking |
| `Step` | 5 packets | additive increase |
| `MinLimit` / `MaxLimit` | 20 / 200 packets | bounds |
| `InitialLimit` | 83 packets | start value (BDP for 10 Mbps, 100 ms) |
| `HardCapFactor` | 2 | hard cap as a multiple of the limit |
| `EnableDynamic` | true | turn Dyn off to get a fixed-limit FSD |

## Work items

### Code

- [ ] Add `src/traffic-control/model/fsd-queue-disc.{h,cc}` (move from scratch, GPL-2.0-only SPDX header, Doxygen comments)
- [ ] Move the `Ctl()` logic into the queue disc; replace the external timer and trace
- [ ] Replace constants with attributes (table above) and `TypeId` registration
- [ ] Register the files in `src/traffic-control/CMakeLists.txt`
- [ ] Keep the flow key from the queue disc item hash; make sure it works with and without packet filters
- [ ] Check behaviour with the other queue disc statistics (`QueueDisc::Stats`, drop reasons). Use a distinct drop reason for FSD drops instead of reusing the generic one

### Tests

- [ ] `src/traffic-control/test/fsd-queue-disc-test-suite.cc` with small deterministic cases: fixed-limit FSD drop rule, hard cap, Dyn shrink after H bad periods, Dyn grow when calm, bounds respected
- [ ] Register the test suite in the test `CMakeLists.txt`
- [ ] Regression check: Dyn + FSD in the new class must reproduce the scratch results (`results/summary.tsv`) for a fixed seed

### Example and docs

- [ ] Small example in `examples/traffic-control/` (one bottleneck, UDP burst plus TCP) instead of the full 5G program
- [ ] Section in `src/traffic-control/doc/` describing the algorithm, attributes and limitations
- [ ] Entry in `CHANGES.md` and `RELEASE_NOTES.md` of the target ns-3 development tree

### Submission

- [ ] Fork `nsnam/ns-3-dev` on GitLab, branch from the current development head (not from the 3.48 tarball)
- [ ] Format with the project style checker (`utils/check-style-clang-format.py`) and run the full test suite
- [ ] Open a merge request with a short description and the key numbers from [../RESULTS.md](../RESULTS.md)
- [ ] Be ready to answer review: why delay-based, why a hold count, comparison with the existing `FifoQueueDisc` / FQ-based discs

## Risks and open points

- Maintainers may prefer Dyn as a separate component or as a generic "adaptive limit" helper instead of inside one disc. The attribute split above keeps that option open.
- The ns-3 development tree may differ from 3.48 in queue disc API. Check against the current head before porting.
- The gain over a BDP-sized static buffer is modest in throughput terms (see RESULTS.md); the case for upstream rests on delay at equal throughput and on the cheap fairness effect, so the example should show exactly that.
- Reviewers will ask for evidence outside the 5G setup. A plain dumbbell example with the same traffic would help.

## Version

| Version | Notes |
|---|---|
| v0 | Scratch program, external controller (current repository state) |
| v1 | Controller inside the queue disc, attributes, tests (target) |
