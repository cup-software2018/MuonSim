# The primary generator

Everything about what to simulate is configured from the macro, with `/gen/vertex`.
The generator takes no command-line options.

- [The one rule](#the-one-rule)
- [Grammar](#grammar)
- [Types](#types)
- [Energy](#energy)
- [Clauses](#clauses)
- [Cosmic muons](#cosmic-muons)
- [Spectrum files](#spectrum-files)
- [Volume names](#volume-names)
- [Examples](#examples)
- [The other /gen commands](#the-other-gen-commands)

## The one rule

**At least one `/gen/vertex` is required.** There is no default generator, and
`/run/beamOn` without one is a fatal error:

```
*** G4Exception : GEN001
no primary vertex defined -- add one with /gen/vertex before /run/beamOn
(cosmic muons are /gen/vertex cosmic input <flux.root>)
```

There used to be an implicit cosmic-muon fallback that read a flux file named on
the command line. It is gone, because `/gen/vertex cosmic input <flux.root>` says
the same thing explicitly and the fallback's file could only ever be set from
outside the macro — so nothing in the macro told you what the run had simulated.

Each `/gen/vertex` **adds** a vertex, and **all of them fire in every event**. A
second command does not replace the first. Every vertex re-samples its own
position, energy and direction each event.

## Grammar

```
/gen/vertex <type> [<energy>] [clause] ...
```

## Types

### A Geant4 particle

Any name Geant4 knows: `e-` `e+` `mu-` `mu+` `gamma` `alpha` `neutron` `proton` …

### An isotope

`60Co` `232Th` `238U` `252Cf` — the `Co60` spelling works too. Leave the energy off
so it starts at rest and decays; the daughter chain is followed. Restrict the chain
with Geant4's own command:

```
/process/had/rdm/nucleusLimits <aMin> <aMax> <zMin> <zMax>
```

> **`252Cf` as a particle name is the ion**, decayed by Geant4, and only ~3% of its
> decays fission. For a fission every event use the `252CfSF` source below.

### A source

A source brings its own particles and energies, so it takes **no energy spec**.

| Source | What | Input file |
|---|---|---|
| `cosmic` | muons from a flux histogram; derives its own position, so it takes no position clause | **required** — `.root` holding the `TH3D` `h_dJdEdTdP` |
| `rockgamma` | gammas from the surrounding rock, entering through the cavern boundary | **required** — `.yml` |
| `IBD` | inverse beta decay → correlated e+ and neutron | **required** — `.yml` |
| `AmBe` | neutron + the 4.44 MeV ¹²C gamma (in 58% of emissions) | *optional* — `.yml` replaces the built-in measured spectrum |
| `252CfSF` | one spontaneous fission: ~3.8 neutrons, ~7 prompt gammas, every event | none — it has three built-in distributions, which one file cannot hold |

Why the asymmetry: `AmBe` has one accepted model, so a built-in spectrum is
honest. `IBD` and `rockgamma` have no single standard — reactor versus
geoneutrino, one rock composition versus another — so they demand the file rather
than quietly inventing one.

Spectra are YAML tables for every source except `cosmic`, whose flux is a 3D joint
distribution in (energy, θ, φ) rather than a table, so it stays a ROOT histogram.

### `file` — events from a text file

For output of a generator this code does not contain: DECAY0 for double beta decay,
or anything else a short script can convert. The file says WHAT came out; the
position clause says WHERE it happened, so one converted file serves any volume of
any geometry.

```
/gen/vertex file input decay0_2nubb.txt involume Crystal
```

The format is line-oriented with two required header lines:

```
# units: energy=MeV time=us
# columns: event particle ke dx dy dz t polx poly polz
0   11             1.5100   0.1230 -0.4560  0.8810     0.0   0 0 0
0   22             0.6093   0.0000  0.0000  1.0000     0.0   0 0 0
0   1000020040     7.6870   0.5774  0.5774  0.5774   164.3   0 0 0
1   opticalphoton  3.1e-6   1.0000  0.0000  0.0000     0.0   0 1 0
```

Rows sharing an `event` value are one event and must be consecutive — the same
grouping the `Escape` tree uses. The example's event 0 is a Bi-Po pair: prompt beta
and gamma, then the alpha 164.3 µs later.

| | |
|---|---|
| `event particle ke dx dy dz` | required, in this order |
| `t` | optional; time within the event, 0 if absent, and must not go backwards |
| `polx poly polz` | optional; polarization, none if absent |

`particle` takes a PDG code or a Geant4 name. Ions go as `10LZZZAAAI` — an alpha is
`1000020040` — or by name.

**`energy=` is required**; there is no default, because keV and MeV are both ordinary
and differ by a thousand. `time=` may be left out and is then `ns`. The units
actually used are printed when the file loads, defaults marked, so a converter that
wrote microseconds and forgot to say so shows up in the log:

```
EventFileGen: 218043 events, 941229 particles from decay0_2nubb.txt
EventFileGen: energy=MeV, time=ns (DEFAULTED, not declared); starting at event 91204
```

Three things to know:

* **Time costs a vertex.** `G4PrimaryParticle` carries no laboratory time, only
  `G4PrimaryVertex` does, so particles at different times land on separate vertices
  at the same position. Simultaneous particles share one.
* **The starting event is random**, and the file cycles. Without that, every task of
  an array job would replay the file from the same end. It also means the order
  events come out is not the order they sit in the file.
* **`rotate` turns each event to a random orientation** as it is used. Off by
  default, because rotating changes the event. Turn it on when a run needs more
  events than the file holds: replaying verbatim repeats the angles *between* the
  particles, which neither a fresh position nor fresh transport decorrelates.
  A decay is isotropic, so the rotation costs nothing — but a file whose directions
  mean something must keep it off.

> **Geant4's optical photon is PDG −22**, which the PDG standard does not define.
> A `-22` from an outside generator probably does not mean an optical photon.
> Write `opticalphoton`.

`onvolume` is refused with this source: the file carries directions and `onvolume`
imposes one, and neither should silently win. Use `point`, `involume` or
`multivolume`.

## Energy

Comes directly after the type. Omit it for 0, i.e. at rest. The unit defaults to
**MeV**, Geant4's native energy unit.

| Form | Distribution |
|---|---|
| `<value> [unit]` | fixed |
| `uniform <min> <max> [unit]` | flat between the bounds |
| `gauss <mean> <sigma> [unit]` | Gaussian, truncated at zero |
| `exp <E0> [unit]` | dN/dE ∝ exp(−E/E0) |
| `powerlaw <index> <min> <max> [unit]` | dN/dE ∝ E^index |
| `spectrum <E>:<w> <E>:<w> … [unit]` | discrete lines, relative weights |

## Clauses

Any order.

### Position — one is required, except for `cosmic`

| Clause | Meaning |
|---|---|
| `point <x> <y> <z> [unit]` | a fixed point; length unit defaults to mm |
| `involume <volume>` | uniform inside that physical volume |
| `onvolume [in\|out\|both] <volume>` | uniform over its surface. `in`/`out` restricts emission to that side of the surface normal; `both` (the default) is 4π |
| `multivolume <pattern>` | every volume whose name contains the pattern, chosen in proportion to cubic volume |

### Direction

| Clause | Meaning |
|---|---|
| `random` | isotropic — the default |
| `direction <dx> <dy> <dz>` | fixed |
| `cos` \| `iso` | how a *restricted surface* spreads its emission. `iso` is uniform in solid angle, right for activity decaying on a surface. `cos` follows Lambert, right for a flux crossing it, and is `rockgamma`'s default |

### Other

| Clause | Meaning |
|---|---|
| `polarization <px> <py> <pz>` | |
| `time <t> [unit]` | vertex time, default ns |
| `input <file>` | data file for the sources above: `.yml`/`.yaml` table, or `.root` histogram |
| `hemisphere <x> <y> <z> <R> [unit]` | the virtual surface `cosmic` starts on — see below |

A vertex outside the world aborts the run reporting the offending coordinates,
rather than crashing inside the navigator.

## Cosmic muons

Muons are launched from a **virtual hemisphere**, dome upwards, of radius `R`
centred on a point you choose:

```
/gen/vertex cosmic input data/muon_flux.root hemisphere 0 0 -460 9500 mm
```

Left unset, the surface falls back to the world's own extent. That keeps a run
working with no configuration, but it ties the acceptance to the geometry, so state
it when the number matters.

**Where the flat side goes is a correctness question, not a tuning one.** Launch
points lie on the dome, so a muon is generated only if it enters the sphere through
the upper half. That is automatic for a steeply falling track, but a near-horizontal
one passing *below* the centre enters through the lower half and is never generated.
So anything sitting under the flat side has an acceptance hole for inclined muons —
put the plane at or below the lowest detector surface.

In this geometry that is `z = -460`, the underside of the PSMD modules lying in the
pit, and `R = 9500` is then set by the farthest detector point, the top corner of
the water tank at 9327 mm. `macro/cosmic_muon.mac` carries the derivation.

How it works, and why the radius is the only knob:

- The flux histogram is sampled for (energy, θ, φ).
- Muons come from above, so a downward-going track that crosses the sphere and
  passes *above* its centre enters through the upper half — the dome. For those the
  dome is the entry surface and every crossing track is generated. (This is the claim
  `CosmicMuonGen.hh` makes without the qualifier, which is why the plane has to sit
  under the detectors: see above.)
- The start point is drawn from a disk of radius `R` perpendicular to the sampled
  direction, then pushed back onto the sphere. Drawing uniformly over that disk is
  the same as drawing uniformly over the hemisphere's projected area, so the cos θ
  weighting comes out automatically — no extra factor is applied, and none should
  be.

So `R` has to enclose whatever the run cares about and nothing larger: the cost
goes as `R²`.

A sphere left too small drops muons that should have been generated, and **nothing
warns** — no overlap check sees it. So `R` has to be re-derived whenever the
apparatus moves. Raising `geo::frame::kTopZ` raises the water tank with it, which is
the binding point, so that constant and this radius are tied together.

> The footpoint displaces from the **detector**, not from the vertical axis. A
> steeply inclined direction still finds a valid start point on the dome; it does
> not lose coverage.

## Spectrum files

The YAML a source reads with `input <file>`:

```yaml
energy_unit: MeV      # optional, default MeV
energy_min: 1.806     # optional sampling-window bounds
energy_max: 10.0
energy: [ ... ]       # required, same length as flux
flux:   [ ... ]       # required, need not be normalised
```

Schema and validation: [src/gen/SpectrumFile.hh](../src/gen/SpectrumFile.hh).
Worked examples ship in `data/`: `ambe_spectrum_example.yml`,
`ibd_spectrum_example.yml`, `rock_gamma_example.yml`.

## Volume names

See [the geometry table in the README](../README.md#physical-volume-names) for the
full list. Two things bite in practice:

**Use `Hall`, not `World`, for anything entering the hall.** `Hall` is the air the
detectors stand in -- the dome and the pit as one volume. The world's own skin is
outside the rock shell, so a vertex there is buried under a metre of rock.

Inside a module, `PSMDScintTop` always looks *away* from what the module shields —
outward for the wall groups, downward for the ones lying in the pit.

**The PSMD module is one logical volume placed 118 times**, so `PSMDScintTop` and
its siblings are shared names — `involume PSMDScintTop` resolves to whichever
module the volume search reaches first, not to all of them. Aim at one module by
its own name, `involume PSMDModule65`.

Useful coordinates are a few metres up, not around the origin: the cavern floor is
`z = 0`, the water tank spans `z = 4550 … 7692`, and the shielding sits between
them.

## Examples

```
# 2.5 MeV electron straight up from a fixed point
/gen/vertex e- 2.5 MeV point 0 0 5000 mm direction 0 0 1

# flat 0-3 MeV electron anywhere in the water, isotropic
/gen/vertex e- uniform 0 3 MeV involume TankWater random

# 60Co decaying in the water: bulk contamination
/gen/vertex 60Co involume TankWater

# 40K gamma from the 48 PMTs, weighted by their volume
/gen/vertex gamma 1.46 MeV multivolume PMTPhys

# 2.615 MeV line smeared by 50 keV
/gen/vertex gamma gauss 2.615 0.05 MeV involume TankWater

# alpha lines with relative intensities
/gen/vertex alpha spectrum 5.407:1 5.685:0.2 MeV involume TankWater

# surface contamination on the steel, emitted into the tank only
/gen/vertex neutron 1 MeV onvolume in TankSteel

# cosmic-like muon spectrum, straight down (note the explicit GeV)
/gen/vertex mu- powerlaw -2.7 1 100 GeV point 0 0 9000 mm direction 0 0 -1

# calibration sources
/gen/vertex AmBe point 0 0 5000 mm
/gen/vertex AmBe involume TankWater input data/ambe_spectrum_example.yml
/gen/vertex 252CfSF involume TankWater
/gen/vertex IBD involume TankWater input data/ibd_spectrum_example.yml

# gammas from the rock, entering through the cavern wall
/gen/vertex rockgamma onvolume in Hall input data/rock_gamma_example.yml

# the cosmic muon generator
/gen/vertex cosmic input data/muon_flux.root

# a muon in a PSMD module in the pit
/gen/vertex mu- 4 GeV involume PSMDModule60 direction 0 0 -1

# two vertices, both firing every event
/gen/vertex e- 1 MeV point -1000 0 5000 mm
/gen/vertex gamma 1 MeV point 1000 0 5000 mm
```

## The other /gen commands

| Command | |
|---|---|
| `/gen/list` | print the vertices defined so far |
| `/gen/clear` | remove them all. At least one must be defined again before `/run/beamOn` |
| `/control/manual /gen/vertex` | the full syntax reference at run time |

Errors are reported with a code and the offending token, e.g. `GEN614` for an
unknown clause, `GEN623` for a missing `input`, `GEN625`/`GEN626` for a bad
`hemisphere`.
