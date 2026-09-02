#pragma once

#include "Observer.hh"

class G4Track;

// Sees each track once before it is tracked and once after.
//
// Between the two other families in cost: the track has been taken off the stack
// and prepared, but has not moved. Kill from Pre with
//
//   track->SetTrackStatus(fStopAndKill);
//
// which is why the track is handed over non-const. For a decision that depends only
// on what the particle IS, StackingObserver is cheaper; use this when the decision
// needs the track as Geant4 has set it up -- its vertex, its creator process, its
// parent -- or when the work belongs at the end of a track rather than at a step.
class TrackingObserver : public Observer {
public:
  using Observer::Observer;

  virtual void PreTracking(G4Track *) {}
  virtual void PostTracking(const G4Track *) {}
};
