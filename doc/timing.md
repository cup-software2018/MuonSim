# Event timing: deferral, the source clock, and pileup

Geant4 runs a decay chain end to end inside one event. Start 238U and all fourteen
decays down to 206Pb arrive together — spread over billions of years of global time,
but in a single `G4Event`.

A detector does not see that. It sees a trigger, an integration window, then another
trigger. Two decays a second apart are two events; two decays 300 ns apart pile into
one. If you are counting *events* rather than *gammas*, the simulation has to be cut
the same way the readout cuts it.

---

## The knobs

```
/gen/window 1 us                     the readout integration time
/gen/activity 1 Bq                   the source's activity
/gen/yieldToDeferred true            one chain in flight at a time
/observer/stack/enable defertrack     split chains at the window
```

There is **no pileup switch**. Pileup is what `activity × window` produces:

| activity | A·window | decays sharing an event |
| --- | --- | --- |
| 1 Bq (default) | 1e-6 | 0.000% |
| 100 kBq | 0.1 | 9.2% |
| 1 MBq | 1 | 50.0% |
| 10 MBq | 10 | 90.9% |

(measured, window 1 µs; the Poisson prediction is `Aτ/(1+Aτ)` = 0.000, 9.091, 50.000,
90.909%). Leave the activity at 1 Bq and pileup never happens; set the real activity
and it happens as often as it should.

---

## Normalisation

The clock schedules each decay a Poisson gap after the last, so at the end of a run
it holds the **live time** the run covers. A rate is:

```
rate = counts / live time
```

Every run prints it:

```
### source: 39980 decays over 0.0398909 s live time at 1e+06 Bq, window 1 us
### divide counts by 0.0398909 s for a rate
### 19980 of those decays piled onto another one (49.975%)
```

and it is on every event as `MCEventInfo::GetUniversalTime()`, in seconds.

**At the default 1 Bq the gaps average one second**, so live time in seconds equals
the number of decays and dividing by it is exactly the per-decay normalisation this
code used before there was a clock. Setting a real activity turns the same division
into a real rate — nothing else changes.

---

## What deferral does

The first decay product in an event opens the window and sets its `t = 0`. Anything
arriving later than `window` after that is classified `fPostpone`, which puts it on
Geant4's own postponed stack; it comes back at the start of the next event with its
state intact.

An event that inherits deferred tracks gets **no new source nucleus**
(`/gen/yieldToDeferred`), so one chain finishes before the next begins. A chain
therefore spans as many events as it needs:

```
[defertrack] event 0  Rn220           parent  1  t = 1.29057 d   -> OPENS the window, t := 0
[defertrack] event 0  Po216           parent  2  t = 31.4473 s   -> DEFER (+31.4473 s)
[defertrack] event 1  Po216           parent -1  t = 31.4473 s   -> in window (same decay)
[defertrack] event 1  Pb212           parent -2  t = 110.295 ms  -> DEFER (+110.295 ms)
[defertrack] event 2  Pb212           parent -1  t = 110.295 ms  -> in window (same decay)
[defertrack] event 2  Bi212[238.632]  parent -2  t = 1.0448 d    -> DEFER (+1.0448 d)
[defertrack] event 3  Bi212[238.632]  parent -1  t = 1.0448 d    -> in window (same decay)
[defertrack] event 3  Bi212           parent -3  t ~ 0           -> in window
[defertrack] event 3  gamma           parent -3  t ~ 0           -> in window
[defertrack] event 3  Po212           parent  1  t = 2.05807 h   -> DEFER (+2.05807 h)
[defertrack] event 4  Po212           parent -1  t = 2.05807 h   -> in window (same decay)
[defertrack] event 4  Pb208           parent -3  t = 129.623 ns  -> in window (+129.623 ns)
```

`/defer/verbose 60` prints that. Returning tracks have a **negative parent id** --
Geant4 marks them so.

Read the last two lines: 212Po decayed 129.6 ns after it was born, inside the 1 µs
window, so it shares event 4 with its own parent's decay. That is the coincidence
this whole mechanism exists to keep. Every other step of the chain -- seconds to
days apart -- landed in an event of its own, which is also right.

The prompt de-excitations show as `t ~ 0` (literally a denormal, a few times 1e-303
picoseconds). Harmless, and a reminder that "same instant" here means same instant.

### The test is on the products, not the daughter nucleus

At stacking time a daughter ion carries the time it was *born*, which is when its
parent decayed — its own decay time has not been sampled yet. But every product of
one decay carries that same decay's time, so testing each product as it is stacked
tests the decay it came from, and the whole decay moves together.

---

## Why times are rebased to zero

This is not cosmetic and it is the reason a chain cannot simply be run from the top.

Started at 232Th, global time reaches 3.8e9 years — 1.2e26 ns. A double resolves
about `2^-52 × 1.2e26 ≈ 2.7e10 ns`, which is **27 seconds**. The 300 ns between
212Bi and 212Po is destroyed inside `G4RadioactiveDecay` when it computes
`t_daughter = t_parent + Δ`, before any observer sees it. Every coincidence test then
answers "same instant" whatever the real gap was, and no post-hoc fix can recover it.

Once the decay that opens the event sits at zero, its daughters' times are sampled
from zero and the nanoseconds survive.

A consequence worth knowing: **recorded times are relative to their event**, not
absolute. The absolute time is the event's `UniversalTime`.

---

## Validated against branching ratios

212Bi branches 64.06% β to 212Po (T½ = 299 ns) and 35.94% α to 208Tl (T½ = 3.05 min).
A 1 µs window catches the 212Po `1 - exp(-ln2 · 1000/299) = 90.2%` of the time, and
never catches 208Tl. That predicts events per chain, and it is measurable:

| window | measured | predicted |
| --- | --- | --- |
| 1 µs | 1.42167 | 1.4224 |
| 100 µs | 1.36047 | 1.3594 |

---

## Three things to watch

**`/gen/yieldToDeferred false` does not give you pileup — it gives wrong pileup.**
Chains mix, but the second one starts at its own `t = 0` rather than a Poisson gap
after the first. Deferred tracks also pile up unprocessed (1.5 million in a 2000-event
test). The run warns:

```
DeferTrackStackingObserver: WARNING -- /gen/yieldToDeferred is off, so events
hold a deferred chain AND a fresh nucleus together
```

**Tracks still deferred when the run ends are lost**, so the last chains are cut
short. Negligible in a long run, fatal in a short one; the count is printed.

**Chain continuations resume in the *next* event, not at their true universal time.**
So pileup between a chain continuation and an independent decay is not modelled.
For a single decay like 2νββ this does not arise; for a chain it is a small effect
against the pileup that is modelled.

---

## For a cryogenic calorimeter

The window is the readout's pulse-pair resolving time — milliseconds for an MMC,
not microseconds. Since pileup goes as `A × window`, that is a factor of 1000 on the
same source. It is the reason 2νββ pileup is a real background where rock-gamma
pileup is not.
