#pragma once

#include <memory>
#include <vector>

#include "G4String.hh"
#include "G4VUserActionInitialization.hh"
#include "EventObserver.hh"
#include "ObserverMessenger.hh"
#include "RootManager.hh"
#include "RunObserver.hh"
#include "StackingObserver.hh"
#include "SteppingObserver.hh"
#include "SteppingAction.hh"
#include "TrackingObserver.hh"

/// Action initialization class.

class ActionInitialization : public G4VUserActionInitialization {
public:
  explicit ActionInitialization(
      const G4String & rootFilename = "muon_output.root",
      std::vector<std::shared_ptr<SteppingObserver>> stepObservers = {},
      std::vector<std::shared_ptr<TrackingObserver>> trackObservers = {},
      std::vector<std::shared_ptr<StackingObserver>> stackObservers = {},
      std::vector<std::shared_ptr<EventObserver>> eventObservers = {},
      std::vector<std::shared_ptr<RunObserver>> runObservers = {});
  ~ActionInitialization() override = default;

  // Set these, or leave them and nothing is labelled. Both are optional and
  // independent: a detector with elements to number but no regions to name sets only
  // the second, and SteppingAction checks each one before calling it.
  //
  // SETTERS RATHER THAN CONSTRUCTOR ARGUMENTS, because the two are the same type --
  // std::function<G4int(const G4Step *)> -- so passing them in the wrong order
  // compiles, runs, and writes each label into the other's field. A name at the call
  // site cannot be got wrong that way. They must be set before /run/initialize,
  // which is when Build() hands them to SteppingAction.
  void SetRegionTagger(SteppingAction::RegionTagger tagger) { fRegionTagger = std::move(tagger); }
  void SetDetectorIDTagger(SteppingAction::DetectorIDTagger tagger)
  {
    fDetectorIDTagger = std::move(tagger);
  }

  void BuildForMaster() const override;
  void Build() const override;

private:
  G4String fRootFilename;
  SteppingAction::RegionTagger fRegionTagger;
  SteppingAction::DetectorIDTagger fDetectorIDTagger;

  // Owned here, for as long as the actions that use them. Handed on as raw
  // pointers because the actions do not share in the ownership.
  std::vector<std::shared_ptr<SteppingObserver>> fSteppingObservers;
  std::vector<std::shared_ptr<TrackingObserver>> fTrackingObservers;
  std::vector<std::shared_ptr<StackingObserver>> fStackingObservers;
  std::vector<std::shared_ptr<EventObserver>> fEventObservers;
  std::vector<std::shared_ptr<RunObserver>> fRunObservers;

  // Registers /observer. Built here because this is what owns the observers, and it
  // exists before any macro runs.
  std::unique_ptr<ObserverMessenger> fObserverMessenger;

  // Every family, as the common base: they all get BeginOfRun and EndOfRun.
  std::vector<Observer *> AllObservers() const;
  RootManager * fRootManager;
};
