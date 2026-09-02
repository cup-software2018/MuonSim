#pragma once

#include <vector>

#include "G4ClassificationOfNewTrack.hh"
#include "G4UserStackingAction.hh"

class G4Track;
class StackingObserver;

// Asks the enabled StackingObservers what to do with each new track.
//
// With none enabled it does nothing at all: every track is classified fUrgent,
// which is what Geant4 does without a stacking action. That is the default, so
// registering this action changes no result.
class StackingAction : public G4UserStackingAction {
public:
  explicit StackingAction(std::vector<StackingObserver *> observers = {});
  ~StackingAction() override = default;

  G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track * track) override;
  void NewStage() override;
  void PrepareNewEvent() override;

private:
  // Not owned: ActionInitialization keeps them alive for the run.
  std::vector<StackingObserver *> fObservers;
};
