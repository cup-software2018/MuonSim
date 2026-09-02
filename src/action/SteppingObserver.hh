#pragma once

#include "Observer.hh"

class G4Step;

// Sees each step after it has been taken.
//
// SteppingAction writes steps into the output and tags them; anything else a
// particular study needs -- a surface flux tally, a custom veto, stopping a track
// once it has been counted -- goes in an observer.
//
// NOT named UserSteppingAction: SteppingAction already IS a G4UserSteppingAction,
// and a second class by that name in one build is a trap for anybody reading a
// stack trace.
//
// The step is const but step->GetTrack() is not, so an observer may kill the track
// with SetTrackStatus(fStopAndKill). SteppingAction records the step BEFORE the
// observers run, so a track killed here still appears in the output for the step on
// which it was killed. This is the DEAREST place to kill -- the track has already
// moved; see StackingObserver for the cheapest.
class SteppingObserver : public Observer {
public:
  using Observer::Observer;

  virtual void Step(const G4Step *) = 0;
};
