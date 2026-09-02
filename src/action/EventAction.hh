#pragma once

#include <vector>

#include "G4UserEventAction.hh"

class EventObserver;
class RootManager;

class EventAction : public G4UserEventAction {
public:
  explicit EventAction(RootManager * rootManager,
                       std::vector<EventObserver *> observers = {});
  ~EventAction() override = default;

  void BeginOfEventAction(const G4Event * event) override;
  void EndOfEventAction(const G4Event * event) override;

private:
  RootManager * fRootManager = nullptr;

  // Not owned. Asked after RootManager at the start of the event and BEFORE it at
  // the end, so an observer can still add to what gets written out.
  std::vector<EventObserver *> fObservers;
};
