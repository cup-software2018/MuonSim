#pragma once

#include <vector>

#include "G4UserTrackingAction.hh"

class RootManager;
class TrackingObserver;
class TrackingAction : public G4UserTrackingAction {

public:
  explicit TrackingAction(RootManager * rootManager,
                          std::vector<TrackingObserver *> observers = {});
  virtual ~TrackingAction() = default;

  virtual void PreUserTrackingAction(const G4Track * track);
  virtual void PostUserTrackingAction(const G4Track * track);

private:
  RootManager * fRootManager;

  // Not owned: ActionInitialization keeps them alive for the run.
  std::vector<TrackingObserver *> fObservers;
};
