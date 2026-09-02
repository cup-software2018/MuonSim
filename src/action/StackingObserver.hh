#pragma once

#include "G4ClassificationOfNewTrack.hh"
#include "Observer.hh"

class G4Track;

// Sees every track as it is created -- primaries and secondaries alike -- BEFORE it
// is tracked, and may discard it.
//
// This is the cheapest place to suppress particles a study does not care about: a
// track killed here costs no step at all, whereas one killed in TrackingObserver has
// already been prepared and one killed in SteppingObserver has already moved. A rock
// gamma run measured 90.7% of its tracks to be electrons against 0.6% gammas, so
// the saving is most of the run.
//
// Cheaper still, where it applies, is not creating the particle: a production cut
// (/run/setCut, or a cut per region) deposits the energy locally instead of making
// a track. Reach for that first and for this second.
//
// AND MIND WHAT THE KILLING COSTS. Discarding electrons in a gamma study also
// discards their bremsstrahlung, so the escaping gamma spectrum loses its softer
// end; discarding alphas removes (alpha, n) reactions, which is fatal to a neutron
// study and harmless to a gamma one. An energy threshold is usually the honest
// version of "this particle does not matter".
class StackingObserver : public Observer {
public:
  using Observer::Observer;

  // Return true and set `classification` to have a say; return false to abstain.
  //
  // Abstention is a distinct answer on purpose. Observers are asked in turn and the
  // FIRST one to answer decides, so an observer that only cares about electrons can
  // say nothing about everything else instead of having to repeat Geant4's default
  // and silently override whatever comes after it. If all abstain, the track is
  // classified fUrgent, which is what Geant4 would have done alone.
  virtual bool Classify(const G4Track * track, G4ClassificationOfNewTrack & classification) = 0;

  // A new event is starting, or the urgent stack has just emptied. Optional.
  virtual void NewStage() {}
  virtual void PrepareNewEvent() {}
};
