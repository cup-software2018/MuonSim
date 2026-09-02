#pragma once

#include <memory>

#include "G4Types.hh"
#include "StackingObserver.hh"

class G4GenericMessenger;

// Splits a decay chain into detector EVENTS instead of letting Geant4 run the whole
// chain as one.
//
// The problem it solves: counting gamma events, not gammas. Geant4 decays a chain
// end to end inside a single event, so 238U and everything down to 206Pb arrives
// together -- fourteen decays spread over billions of years of global time, all in
// one G4Event. A detector does not see that. It sees a trigger, an integration
// window of order a microsecond, and then another trigger. Two decays a second
// apart are two events; two decays 300 ns apart pile up into one.
//
// What this does: the first decay product seen in an event opens the window and
// defines its t = 0. Anything arriving more than `window` after that is classified
// fPostpone, which hands it to Geant4's deferred stack and brings it back at the
// start of the NEXT event, its global time intact. So each event holds exactly the
// decays that a detector would have integrated together.
//
// WHY THE TEST IS ON THE PRODUCTS, not on the daughter nucleus. At stacking time a
// daughter ion carries the time it was BORN, which is when its parent decayed --
// its own decay time has not been sampled yet. But every product of one decay,
// nucleus and gammas alike, carries that same decay's time, so testing each product
// as it is stacked tests the decay it came from, and the whole decay moves together.
// Isomers fall out correctly too: a metastable daughter is its own ion track, and
// its de-excitation gamma carries the later time.
//
// THIS CHANGES WHAT AN EVENT IS, and two things follow that the caller must handle:
//
//   - An event that inherits deferred tracks should NOT also get a fresh primary,
//     or one event holds two unrelated chains. PrimaryGeneratorAction skips
//     generating while the deferred stack is occupied -- see /gen/deferred.
//   - The number of events therefore stops being the number of source nuclei, so
//     anything normalising per decay must count primaries, not events.
//
// It reproduces TRUE coincidence, a chain decaying within one window. Accidental
// coincidence between unrelated atoms is a function of the total activity and is
// not simulated here at all.
class DeferTrackStackingObserver : public StackingObserver {
public:
  explicit DeferTrackStackingObserver(G4double window = 1000. /* ns */);
  ~DeferTrackStackingObserver() override;

  std::string Describe() const override;

  bool Classify(const G4Track * track, G4ClassificationOfNewTrack & classification) override;

  void PrepareNewEvent() override;
  void BeginOfRun(const G4Run * run, RootManager * root) override;
  void EndOfRun(const G4Run * run, RootManager * root) override;

  void SetWindow(G4double window) { fWindow = window; }
  G4double GetWindow() const { return fWindow; }

  // Prints the first N tracks it judges, with times and decisions. This is how the
  // defer-and-return behaviour was checked in the first place, and it is the
  // only way to see the window working on a specific chain.
  void SetVerbose(G4int n) { fVerbose = n; }

private:
  G4double fWindow;

  bool fTriggerSet = false;
  G4double fTriggerTime = 0.;

  long long fEvents = 0;
  long long fInWindow = 0;
  long long fDeferred = 0;
  long long fEventsWithNoTrigger = 0;

  G4int fVerbose = 0;
  long long fPrinted = 0;

  std::unique_ptr<G4GenericMessenger> fMessenger;
};
