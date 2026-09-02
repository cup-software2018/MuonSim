#pragma once

#include <vector>

#include "G4Accumulable.hh"
#include "G4UserRunAction.hh"

class G4Run;
class RootManager;
class Observer;
class RunAction : public G4UserRunAction {
public:
  explicit RunAction(RootManager * rootManager,
                     std::vector<Observer *> observers = {});
  ~RunAction() override = default;

  void BeginOfRunAction(const G4Run * run) override;
  void EndOfRunAction(const G4Run * run) override;

private:
  RootManager * fRootManager = nullptr;

  // Not owned. Given a chance to open and close whatever they write.
  std::vector<Observer *> fObservers;
};
