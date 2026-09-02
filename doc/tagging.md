# Tagging: labelling steps from the geometry

The library records tracks and steps without knowing what any volume is for — that
is what keeps `src/action`, `src/edm` and `src/phys` reusable. So when you want the
output to say *where* something happened, the geometry has to supply the labelling.

You give it one function. The library calls it on every step and stores the integer
it returns, without interpreting it.

---

## Region tagging in four pieces

Say your detector has a veto panel, a shield and a crystal, and you want each track
to remember which of them it passed through.

**1. Define your bits** — in your own detector header, one bit per region:

```cpp
enum Region : int {
  kNone    = 0,
  kVeto    = 1 << 0,
  kShield  = 1 << 1,
  kCrystal = 1 << 2
};
```

**2. Write the tagger** — return the bit for wherever this step is:

```cpp
std::function<G4int(const G4Step *)> MyDetector::MakeRegionTagger() const
{
  return [](const G4Step * step) -> G4int {
    const G4String & name = step->GetPreStepPoint()->GetPhysicalVolume()->GetName();
    if (name == "Veto")    return kVeto;
    if (name == "Shield")  return kShield;
    if (name == "Crystal") return kCrystal;
    return kNone;                       // not a region we care about
  };
}
```

**3. Hand it over** — where the run is wired up, in your `main`:

```cpp
new ActionInitialization(outputFile, detector->MakeRegionTagger(), ...)
```

**4. Ask for tracks** in the macro, or there is nothing to store the label on:

```
/ROOT/savetrackopt 1
```

That is all. The library ORs your bit onto `MCTrack::fRegionMask` for every step of
the track.

## Reading it back

```cpp
const int mask = track->GetRegionMask();

if (mask & kVeto)                    // this track was in the veto at some point
if ((mask & kCrystal) && !(mask & kVeto))   // reached the crystal without a veto hit
if (mask == 0)                       // never in a tagged region
```

The mask is a **set, not a history**: it says which regions the track visited, not in
what order or for how long. For energy per region, sum the steps instead.

---

## Two things that catch people

**The bit needs its particle saved.** The mask lives on `MCTrack`, so if the track
was not kept there is nowhere to put it and the bit is silently dropped.
`/ROOT/savetrackopt` decides:

| value | keeps |
| --- | --- |
| 1 | muons, hadrons, ions |
| 2 | e± not from EM showers |
| 4 | gammas |
| 8 | e± from showers |
| 16 | optical photons |

OR them together (`7` = the first three). With `savetrackopt 0` the tagger still runs
and everything it computes is thrown away.

**If your tagger reads a detector member, capture `this` — not the member.** The
tagger is built while the run is being wired up, which is *before* `/run/initialize`
builds the geometry, so any dimension you copy in is still zero:

```cpp
return [this](const G4Step * step) -> G4int {
  const double top = fRoomTopZ;        // read at STEP time, after Construct()
  ...
};
```

Capturing `fRoomTopZ` by value compiles, runs, and compares everything against zero.

---

## The sibling: detector id

The same idea, one function along the same path, but stored per **step** instead of
per track:

```cpp
std::function<G4int(const G4Step *)> MyDetector::MakeDetectorIDTagger() const
{
  return [](const G4Step * step) -> G4int {
    const G4StepPoint * pre = step->GetPreStepPoint();
    const G4VPhysicalVolume * pv = pre ? pre->GetPhysicalVolume() : nullptr;
    if (!pv || pv->GetName() != "Crystal") return -1;      // -1 means "no element"
    return pre->GetTouchableHandle()->GetCopyNumber();     // which crystal
  };
}
```

It lands in `MCStep::fDetectorID`, overwritten per step rather than accumulated, and
answers "which element was hit" rather than "where did this track go".
