# Review 03 performance measurements

Measured on 2026-10-04 with the Windows x64 MSVC Release tiles test build and
the capture backend.  The production source baseline was `3688682307`.
These measurements support deferring O03-01 and O03-02's additional caches or
selection revision tracking.  They do not establish NVDA responsiveness or
complete Stage 22's gameplay acceptance.

## Reproduce

Run from the repository root:

```bat
test_all.bat "[accessibility_profile]" --benchmark-samples 10
```

The hidden cases in `tests/accessibility_benchmark_test.cpp` are independent
of the normal suite.  The selection kernel uses Catch2's sampling.  The wait
and crowd profiles print three runs per speech mode, component times, and
backend counts.  Do not run another build or CPU-heavy test during measurement.
Timing results are diagnostic; there are no machine-dependent timing assertions.

## O03-01: Unchanged selection

The fixture creates real map items and marks them through the multiselector's
selection operations.  It measures the guarded copy and comparison used by
`inventory_multiselector::on_input()` separately from navigation, rendering,
and selection mutations.

| Selected item locations | Speech On, mean | Speech Off, mean |
| --- | --- | --- |
| 10 | 0.329 microseconds | 0.005 microseconds |
| 100 | 2.873 microseconds | 0.005 microseconds |
| 1,000 | 32.278 microseconds | 0.005 microseconds |

The 1,000-item sample has a wider confidence interval (mean bounds about
29.7–40.9 microseconds).  Linear scaling is real, but the measured cost is
about 0.03 milliseconds per unchanged input.  Keep the reliable snapshot until
a larger representative workload or whole-input profile shows a material cost.
A replacement must still detect quantity changes, bulk marks, deselection,
and changed identities; selected-item count alone is insufficient.

## O03-02: Eight-hour wait

The fixture executes 28,800 `ACT_WAIT` turns with the accessibility calls made
during an activity.  It includes timestamp instrumentation but omits the rest
of the game turn, creature AI, rendering, and an actual keyboard event loop.

| Component | Speech On, microseconds/turn | Speech Off, microseconds/turn |
| --- | --- | --- |
| Activity update, including speech scope and first progress update | 6.86–6.90 | 5.22–5.23 |
| Threat and condition checks | 1.68–1.70 | 0.025–0.026 |
| Second progress update | 1.37–1.38 | 0.025–0.026 |
| Flush and poll, combined | about 0.08 | about 0.05 |

Total elapsed time was 292–294 milliseconds On and about 158 milliseconds Off:
roughly 135 milliseconds of additional work over eight simulated hours, or
4.7 microseconds per turn.  Speech On produced only the start and completion
announcements; Off produced none.  Neither mode called backend cancellation.

The main added costs are activity/progress work and condition input collection,
not backend calls.  Defer additional caching at these measured costs: progress
must remain current on repeat, and urgent conditions can change during every
turn.  The isolated ratio is not a whole-game slowdown estimate.

## O03-03: Changing visibility with 60 creatures

The fixture uses 60 real zombies and the avatar's visibility checks in daylight.
A 43-step route crosses from behind a wall into full sight and back, with ten
changes in visible count and a verified maximum of 60.  It rebuilds the real
map/visibility caches, calls movement and threat hooks, and flushes speech.
Creatures remain stationary; the profile does not simulate combat or their AI.

- Speech On hooks: mean 29.8–31.0 microseconds per step; observed maxima
  56.7–84.7 microseconds.  Off: mean 0.86–0.92 microseconds per step.
- Cache rebuilding and visibility checks take about 15.1–15.2 milliseconds
  per step.  Total route time is about 650–655 milliseconds On and
  654–657 milliseconds Off; this small difference is within run variation.
- On produces 43 backend announcements, at most one combined call per step;
  Off produces none.  Neither mode cancels speech.  An unchanged threat
  recheck adds no calls, and repeat returns the latest meaningful announcement.

The capture CPU cost is small compared with this visibility workload.  Actual
input-to-speech latency, queued-speech freshness, interruption, and blind-player
acceptance remain unverified in `ACCESS_USERTESTS.md`, scenario T185.
