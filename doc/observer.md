# Observers: putting your own code in the loop

The library owns the `G4User*Action` classes. You do not edit them — you write an
observer, and it gets called at the point you care about. Adding a study is adding
one file.

Every observer has a name, starts **off**, and is switched on from the macro.

---

## The five families

| family | you implement | called |
| --- | --- | --- |
| `RunObserver` | (nothing extra) | start and end of the run |
| `EventObserver` | `BeginOfEvent` / `EndOfEvent` | around each event |
| `StackingObserver` | `Classify` | a new track is about to be stacked |
| `TrackingObserver` | `PreTracking` / `PostTracking` | around each track |
| `SteppingObserver` | `Step` | after each step |

All of them also inherit `BeginOfRun` / `EndOfRun` from `Observer`, for opening and
closing whatever they write.

Pick by *when* you need to act. Killing a track is cheapest in `Classify` (it never
gets tracked) and dearest in `Step` (it has already moved).

---

## A first observer

Count how much energy lands in a volume. One file, nothing else to touch:

```cpp
// src/user/CrystalDoseObserver.cc
#include "G4Step.hh"
#include "G4ios.hh"
#include "ObserverRegistry.hh"
#include "SteppingObserver.hh"

namespace {

class CrystalDose : public SteppingObserver {
public:
  CrystalDose() : SteppingObserver("crystaldose") {}      // the macro name

  std::string Describe() const override { return "sums energy deposited in Crystal"; }

  void Step(const G4Step * step) override
  {
    const auto * pv = step->GetPreStepPoint()->GetPhysicalVolume();
    if (pv && pv->GetName() == "Crystal") fEdep += step->GetTotalEnergyDeposit();
  }

  void EndOfRun(const G4Run *, RootManager *) override
  {
    G4cout << "CrystalDose: " << fEdep / CLHEP::MeV << " MeV" << G4endl;
    fEdep = 0.;
  }

private:
  G4double fEdep = 0.;
};

// Announces itself, so main names nothing.
const bool registered = ObserverRegistry<SteppingObserver>::Add(
    "crystaldose", [] { return std::make_shared<CrystalDose>(); });

} // namespace
```

Then in the macro:

```
/observer/list                        what this build has, and what is on
/observer/step/enable crystaldose
/run/beamOn 1000
```

`cmake` picks the new file up on its next run — the source list is a glob, so a new
file needs `cmake` re-run before `make`.

---

## Writing to the output file

Do not open a file. Ask `RootManager` in `BeginOfRun`, and everything lands in the
run's one output file:

```cpp
void BeginOfRun(const G4Run *, RootManager * root) override
{
  fTree = root->AddTree("Dose", "energy per event");
  fTree->Branch("edep", &fEdep);

  root->Add(new TParameter<double>("threshold", fThreshold));   // written and deleted for you
}
```

---

## The stacking family answers a question

`Classify` is the one family whose return value matters. It decides what happens to a
track before it is tracked at all:

```cpp
bool Classify(const G4Track * track, G4ClassificationOfNewTrack & classification) override
{
  if (track->GetDefinition() == G4Neutron::Definition()) {
    classification = fKill;
    return true;                 // I decided
  }
  return false;                  // I have no opinion -- ask the next observer
}
```

Returning `false` is **abstaining**, and it is different from saying "keep it". The
first observer to return `true` decides; if all abstain, Geant4's default applies. So
an observer that only cares about neutrons says nothing about anything else, instead
of having to repeat the default and silently override whoever comes after it.

Order of precedence is registration order, which `/observer/list` prints.

---

## Two things that catch people

**Enable before `/run/beamOn`.** Observers are handed the output at the start of the
run, so enabling one afterwards does nothing for that run.

**A static library can swallow your observer, silently.** Registration happens in a
static initialiser, and that only runs if the linker pulled the object file in.
Nothing references `CrystalDoseObserver.cc` — that is the point of self-registration
— so in a static library it can be dropped, and the observer just is not in
`/observer/list`. No error, no warning. Keep user observers in the executable's own
sources, or force the whole archive in.

**Killing has a cost you should state.** An observer that discards electrons also
discards their bremsstrahlung; one that discards alphas removes (α,n) reactions.
Neither is wrong, but the run is no longer what it looks like, which is why nothing
is on by default.
