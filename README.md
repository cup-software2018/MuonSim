# MuonSim

Geant4 simulation of the AMoRE muon detectors — the water Cherenkov detector
(WCMD) and the plastic scintillator detector (PSMD) — with the passive shielding,
the steel frame, and the rock cavern around them.

Everything except the geometry is reusable. The data model, the actions, the
generators and the material loader name no detector and can be linked by another
experiment's application; `src/user` is the one directory a new experiment
rewrites. See [Repository layout](#repository-layout).

## Where to look

This file covers building, running and the output format. Anything with depth to it
has a document of its own:

| | |
| --- | --- |
| **[doc/generator.md](doc/generator.md)** | `/gen/vertex` — sources, positions, energy spectra |
| **[doc/observer.md](doc/observer.md)** | `/observer/…` — putting your own code in the run, event, stacking, tracking or stepping loop |
| **[doc/tagging.md](doc/tagging.md)** | how the geometry labels steps for a library that knows no volumes |
| **[doc/timing.md](doc/timing.md)** | `/gen/window`, `/gen/activity` — decay chains into detector events, the source clock, pileup |

For the geometry itself, read [src/user/GeometryConstants.hh](src/user/GeometryConstants.hh):
every dimension is there with its provenance, and the ones still waiting for survey
data are marked `PROVISIONAL` or `ASSUMED`.

Contents here: [Requirements](#requirements) · [Build](#build) ·
[Running](#running) · [Options](#options) ·
[Where settings live](#where-settings-live-and-why) ·
[Macro commands](#macro-commands) · [Output](#output) ·
[Optical properties](#optical-properties) · [Data lookup](#data-file-lookup) ·
[Repository layout](#repository-layout)

## Requirements

| | |
|---|---|
| CMake | 3.16 or newer |
| Geant4 | built with `ui_all vis_all` for the interactive session |
| ROOT | components `Core Hist RIO Tree` |
| yaml-cpp | for the material-property and spectrum files |

## Build

```sh
cmake -S . -B build
cmake --build build -j8
(cd build && ctest)
```

The three tests link the generator library directly and need no geometry or run
manager, so they run in a second.

The source list is a glob, so **a new file needs `cmake -S . -B build` re-run**
before `make` sees it — this is how a self-registering observer goes missing.

Installing gives a relocatable tree with a setup script:

```sh
cmake --install build --prefix /path/to/install
source /path/to/install/setup_muonsim.sh
```

which puts the executable on `PATH`, the libraries on `LD_LIBRARY_PATH`, and sets
`MUONSIM_DATA` so the shipped data files are found from anywhere.

## Running

```sh
MuonSim macro/run.mac                        # batch
MuonSim -n 1000 -s 12345 -o out.root macro/run.mac
MuonSim                                      # interactive, runs vis.mac
MuonSim --help
```

`-n 1` builds the geometry, runs the overlap check and fires one event — the cheap
way to check a geometry change or a material file. `-n 0` builds and checks but
writes no output: with no events the run actions never fire, so nothing books a
tree.

Batch launchers for the two production studies are in `run/`:
`run_cosmic.sh <jobid>` and `run_rockgamma.sh <jobid> <isotope>`. Both take their
configuration from an edit-in-file block at the top and log what they ran, the
binary's build date included.

## Options

```
MuonSim [options] [macro]

  -m, --material <file>   optical property YAML. Repeatable, each file adding to
                          the ones before. Defaults to the shipped file.
  -g, --geometry <file>   geometry YAML. RESERVED — no loader exists yet, and a
                          run started with one refuses rather than ignoring it.
  -o, --output <file>     output ROOT file  [muon_output.root]
  -n, --nevent <n>        events to generate [10000]. Reaches the macro as the
                          alias {nevent}.
  -s, --seed <n>          random seed, 1..900000000. Omitted or 0 seeds from the
                          clock.
  -h, --help
  macro                   the macro to run. With none, an interactive session
                          starts and executes vis.mac.
```

Options may come before or after the macro name.

`-n` arrives as a macro alias rather than as a `/run/beamOn` from `main`, so a
macro that already fires the beam does not run twice. A macro cooperates by writing
`/run/beamOn {nevent}`; one that hardcodes a count is simply unaffected.

`-s` with a fixed value makes a run reproducible. Two runs at the same seed give
byte-identical output size; the file's md5 still differs, because ROOT stamps a
UUID into the header.

## Where settings live, and why

Three tiers, and the split is not a matter of taste.

| Tier | What | Why there |
|---|---|---|
| **Must be an option** | `-m`, `-g` | They have to be in place before `/run/initialize` builds the geometry. A macro command that only works above a certain line is a trap — see below. |
| **May be an option** | `-o`, `-s`, `-n` | Exactly what differs between jobs of one batch scan. The scan loop lives in the shell. |
| **Macro** | generator, observers, timing, save options, physics, verbosity | What the run *does*, in the order it does it. |

The trap is not hypothetical. `/material/load` used to exist alongside `-m`. Issued
after `/run/initialize` it registered the file and **nothing ever applied it** — no
warning, exit 0, and every optical number in the run came from somewhere else. The
command is gone; naming a property file too late is now fatal (`MAT001`), and `-g`
likewise refuses (`GEO005`) rather than being ignored.

## Macro commands

The macro is where a run is configured. Each family is documented separately:

| | | |
|---|---|---|
| `/gen/vertex …` | what to shoot, from where | [generator.md](doc/generator.md) |
| `/gen/window`, `/gen/activity`, `/gen/yieldToDeferred` | how long an event lasts, how active the source is | [timing.md](doc/timing.md) |
| `/observer/{run,event,stack,track,step}/enable <name>` | switch on code of your own | [observer.md](doc/observer.md) |
| `/observer/list` | what this build has, and what is on | |
| `/ROOT/save*` | what reaches the file | [below](#what-gets-saved) |

Every observer starts **off**: being linked in must not be the same as being asked
for, because one that kills tracks changes the physics.

## Output

One ROOT file holding a `TTree` named `Event`:

| Branch | Class | Holds |
|---|---|---|
| `MCEventInfo` | `MCEventInfo` | event number, and the source clock in seconds |
| `MCPrimaryData` | `MCPrimary` | the primaries the generator made |
| `MCTrackData` | `MCTrack` | tracks, each owning its `MCStep`s |
| `MCPMTData` | `MCPMT` | per-PMT hits, optionally each photon |
| `MCScintData` | `MCScint` | one entry per scintillator that saw energy |

`MCScint` is an aggregate, not a hit list: every deposit in that scintillator is
summed into it, including energy carried off by tracks the run chose not to save.
It also carries the Birks-quenched sum (`fEnergyDepositVisible`) and the
first-deposit time and position.

Observers may add trees and objects of their own to the same file — see
[observer.md](doc/observer.md).

`libSimData` links only ROOT, not Geant4, and its rootmap autoloads, so an analysis
needs nothing but the file. Worked examples are in `run/`: `compare.py` and
`wcmd.py` (PyROOT to read, numpy and matplotlib for the rest), and the ROOT macros
`generated_muon.C`, `rock_gamma_position.C`, `rock_gamma_shape.C`.

### What gets saved

Two bit masks, set from the macro. Same bit positions in both:

| Bit | Name | Tracks |
|---|---|---|
| 1 | `kSaveHeavy` | muons, hadrons, ions |
| 2 | `kSaveElec` | e± *not* from EM-shower processes — decay electrons land here |
| 4 | `kSaveGamma` | gammas |
| 8 | `kSaveEMShower` | e± from compt/phot/conv/annihil/Ioni/Brem |
| 16 | `kSaveOptical` | optical photons |

```
/ROOT/savetrackopt 1      # which tracks get an MCTrack
/ROOT/savestepopt 1       # which of those also get their steps
/ROOT/savehitphoton 1     # per-PMT photon detail
/ROOT/savescintstep 0     # per-deposit rows inside MCScint
```

A step is stored inside its `MCTrack`, so the effective step set is
`savetrackopt & savestepopt`. **Anything hung off a track needs its bit** —
`MCTrack::fRegionMask` included, which is why a region tagger produces nothing at
`savetrackopt 0`.

Gammas have a bit of their own because they dominate both the interest and the
size: 300 cosmic events at a fixed seed give 2.39 MB at `savetrackopt 1` and
9.57 MB at `5`. Ask for them deliberately.

`MCScint` is filled regardless of these masks. Gating it would lose the energy
carried by unsaved tracks — about a fifth of it, measured.

## Optical properties

Refractive indices, absorption and Rayleigh lengths, reflectivity and photocathode
efficiency come from YAML, not from code — they are the systematics of an optical
detector, so varying one must not need a rebuild.

```sh
MuonSim -m my_materials.yml run.mac
MuonSim -m base.yml -m water_20pct_worse.yml run.mac   # the second overrides
```

With none given, `data/optical/wcmd_materials.yml` is used. It is not optional:
without it the water has no refractive index and the detector makes no light, so a
missing file is fatal rather than a quiet run with no Cherenkov.

Every run prints the file and a content digest, so an output can be traced back to
the numbers that produced it:

```
Material properties: .../wcmd_materials.yml -- 11 tables, 0 constants, digest 2470dab087f140ee/6396B
```

The schema — wavelength or energy input, unit handling, the λ² Jacobian for
emission spectra, Birks constants, and why unknown keys are rejected — is at the
top of [src/phys/MaterialPropertyFile.hh](src/phys/MaterialPropertyFile.hh).

## Data file lookup

Any data file named without a path is looked for in this order:

1. the path as given
2. `$MUONSIM_DATA/<path>`
3. `<executable>/../data/<path>`
4. the compiled-in install `data` directory

So a build tree and an installed tree both work with no configuration.

## Repository layout

The library/detector boundary is deliberate: nothing under the library directories
names a detector.

| Path | | |
|---|---|---|
| `src/edm` | library | the ROOT data model; links no Geant4 |
| `src/action` | library | run/event/tracking/stepping actions, the observer framework, `RootManager` |
| `src/gen` | library | primary generators and the `/gen/vertex` parser |
| `src/phys` | library | physics list, optical surfaces, YAML loaders, data paths |
| `src/pmt` | library | PMT fast simulation |
| `src/user` | **replace this** | `DetectorConstruction`, `GeometryConstants.hh`, `MuonSim.cc`, and any observers of its own |
| `data/` | | shipped YAML and flux files |
| `macro/` | | `run.mac`, `vis.mac`, `cosmic_muon.mac`, `rockgamma.mac`, `rockgamma_batch.mac` |
| `run/` | | batch launchers and analysis macros |
| `doc/` | | the documents listed at the top |

The library reaches into `src/user` two ways and no other:

* **two taggers**, set on `ActionInitialization` — see [tagging.md](doc/tagging.md)
* **five observer families**, which register themselves — see
  [observer.md](doc/observer.md)

Terminology follows the same line: a scintillator id is `scint` in the library and
`slab` in `src/user`; `channel` is reserved for the readout level that does not
exist yet.

Consequence worth knowing: `src/user` headers are **not installed**, so an analysis
program cannot ask the geometry what a detector id or an `fRegionMask` bit means.
Interpreting those still requires reading `src/user`. Giving the library a contract
for this is open work.
