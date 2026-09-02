#include <iomanip>
#include <sstream>

#include "G4GenericMessenger.hh"
#include "G4EventManager.hh"
#include "G4RunManager.hh"
#include "G4StackManager.hh"
#include "G4Run.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"
#include "G4Track.hh"
#include "G4ios.hh"

#include "DeferTrackStackingObserver.hh"
#include "PrimaryGeneratorAction.hh"
#include "ObserverRegistry.hh"

DeferTrackStackingObserver::DeferTrackStackingObserver(G4double window)
  : StackingObserver("defertrack"),
    fWindow(window)
{
  // No window command here. The window belongs to /gen/window, on the generator,
  // because the generator is what groups decays into an event -- two commands for
  // one boundary is two ways to disagree about it.
  fMessenger = std::make_unique<G4GenericMessenger>(this, "/defer/", "decay chain splitting");
  fMessenger->DeclareMethod("verbose", &DeferTrackStackingObserver::SetVerbose,
                            "print the first N stacking decisions, with times");
}

DeferTrackStackingObserver::~DeferTrackStackingObserver() = default;

namespace {

const bool registered = ObserverRegistry<StackingObserver>::Add(
    "defertrack", [] { return std::make_shared<DeferTrackStackingObserver>(); });

} // namespace

std::string DeferTrackStackingObserver::Describe() const
{
  std::ostringstream out;
  out << "one detector event per " << G4BestUnit(fWindow, "Time")
      << " of decay chain; later decays deferred to the next event";
  return out.str();
}

void DeferTrackStackingObserver::PrepareNewEvent()
{
  if (fEvents > 0 && !fTriggerSet) fEventsWithNoTrigger++;
  fTriggerSet = false;
  fEvents++;
}

bool DeferTrackStackingObserver::Classify(const G4Track * track,
                                        G4ClassificationOfNewTrack & classification)
{
  // The source nuclide itself, placed by the generator. It carries the generator's
  // time, not a decay's, so it must not open the window -- 222Rn sits for days
  // before it decays, and that wait is not part of any event.
  if (track->GetParentID() == 0) return false;

  // Non-const so the time can be rebased. The track has not been tracked yet, and
  // TrackingAction already const_casts for the same reason: the const in the hook
  // is about not rewriting history, not about the decision.
  G4Track * mutableTrack = const_cast<G4Track *>(track);
  const G4double t = mutableTrack->GetGlobalTime();

  const bool tell = fPrinted < fVerbose;
  if (tell) {
    fPrinted++;
    G4cout << "  [defertrack] event " << fEvents - 1 << "  " << std::setw(10)
           << track->GetDefinition()->GetParticleName() << "  parent " << std::setw(4)
           << track->GetParentID() << "  t = " << std::setw(12) << G4BestUnit(t, "Time");
  }

  // First product of the first decay in this event: it IS the trigger, and its time
  // becomes the event's zero.
  //
  // REBASING IS NOT COSMETIC. Started from the top of a chain, Geant4's global time
  // reaches 1e10 years, where a double resolves about 30 seconds -- 1.2e26 ns times
  // 2^-52. The 300 ns between 212Bi and 212Po is destroyed inside G4RadioactiveDecay
  // before any observer sees it, and every coincidence test then answers "same
  // instant" whatever the real gap was. Once the opening decay sits at zero, its
  // daughters' times are sampled from zero and the nanoseconds survive.
  if (!fTriggerSet) {
    fTriggerSet = true;
    fTriggerTime = t;
    mutableTrack->SetGlobalTime(0.);
    fInWindow++;
    if (tell) G4cout << "   -> OPENS the window, t := 0" << G4endl;
    return false;
  }

  // A sibling: another product of the very decay that opened the event. Geant4
  // stacks all of one decay's secondaries together, so they carry that decay's
  // absolute time -- the SAME double, which is why comparing for equality is exact
  // rather than fragile. Everything else reaching this point was born from a parent
  // that has already been rebased, so its time is already relative to the trigger
  // and must NOT have the offset taken off it twice. Doing that was the first
  // version of this, and it made every gap negative and every decay "in window".
  if (t == fTriggerTime) {
    mutableTrack->SetGlobalTime(0.);
    fInWindow++;
    if (tell) G4cout << "   -> in window (same decay)" << G4endl;
    return false;
  }

  if (t > fWindow) {
    // Left as it is: it is rebased when it comes back and opens its own event.
    // fPostpone is Geant4's spelling of the same idea; "defer" is ours everywhere
    // else, so the two words meet exactly here and at GetNPostponedTrack below.
    classification = fPostpone;
    fDeferred++;
    if (tell) G4cout << "   -> DEFER (+" << G4BestUnit(t, "Time") << ")" << G4endl;
    return true;
  }

  fInWindow++;
  if (tell) G4cout << "   -> in window (+" << G4BestUnit(t, "Time") << ")" << G4endl;
  return false;
}

void DeferTrackStackingObserver::BeginOfRun(const G4Run *, RootManager *)
{
  fEvents = fInWindow = fDeferred = fEventsWithNoTrigger = 0;
  fTriggerSet = false;

  // Taken from the generator at the start of every run, so /gen/window can be set
  // anywhere in the macro and both halves still see the same number.
  if (auto * runManager = G4RunManager::GetRunManager()) {
    if (const auto * generator = dynamic_cast<const PrimaryGeneratorAction *>(
            runManager->GetUserPrimaryGeneratorAction()))
      fWindow = generator->GetEventWindow();
  }
}

void DeferTrackStackingObserver::EndOfRun(const G4Run *, RootManager *)
{
  if (fEvents == 0) return;

  G4cout << "DeferTrackStackingObserver: window " << G4BestUnit(fWindow, "Time") << ", " << fEvents
         << " events, " << fInWindow << " tracks kept in window, " << fDeferred
         << " deferred to a later event" << G4endl;

  // Whatever is still waiting when the run ends never gets an event, so the last
  // chains in a run are truncated. Small against a long run and fatal against a
  // short one, which is why it is a count and not a footnote.
  if (auto * eventManager = G4EventManager::GetEventManager()) {
    if (auto * stackManager = eventManager->GetStackManager()) {
      const G4int stranded = stackManager->GetNPostponedTrack();
      if (stranded > 0)
        G4cout << "DeferTrackStackingObserver: " << stranded
               << " tracks were still deferred when the run ended -- those chains are cut short"
               << G4endl;
    }
  }

  // The number a rate per decay must be divided by. It is NOT the event count once
  // a chain is being split, and the two differ by however often the chain needed a
  // second event -- printed here so nobody has to reconstruct it afterwards.
  if (auto * runManager = G4RunManager::GetRunManager()) {
    const auto * generator =
        dynamic_cast<const PrimaryGeneratorAction *>(runManager->GetUserPrimaryGeneratorAction());
    if (generator) {
      const G4long planted = generator->GetPrimariesGenerated();
      G4cout << "DeferTrackStackingObserver: " << planted << " source nuclei planted in " << fEvents
             << " events";
      if (planted > 0) G4cout << " = " << double(fEvents) / planted << " events per decay chain";
      G4cout << G4endl;
      if (!generator->GetYieldToDeferred())
        G4cout << "DeferTrackStackingObserver: WARNING -- /gen/yieldToDeferred is off, so events"
               << " hold a deferred chain AND a fresh nucleus together" << G4endl;
    }
  }

  // An event that opened no window held no decay at all. A few are normal -- an
  // event whose deferred tracks all came from one decay that then ended the chain
  // -- but a large number means the source is not decaying, which is worth saying
  // out loud rather than leaving as a quiet zero in the output.
  if (fEventsWithNoTrigger > 0)
    G4cout << "DeferTrackStackingObserver: " << fEventsWithNoTrigger
           << " events contained no decay product at all" << G4endl;

  fEvents = fInWindow = fDeferred = fEventsWithNoTrigger = 0;
}
