#pragma once

#include <string>
#include <utility>

class G4Run;
class RootManager;

// What every observer family has in common: a name a macro can address, an on/off
// flag, and a chance to open and close whatever it records.
//
// Five families derive from this, one per point where Geant4 lets user code in:
//
//   RunObserver        the run itself; adds nothing to this base, and exists so an
//                      observer wanting only these hooks has a way to be registered
//   EventObserver      around each event, once everything in it is tracked
//   StackingObserver   a track is about to enter the stack   -- cheapest place to kill
//   TrackingObserver   a track is about to be, or has been, tracked
//   SteppingObserver   a step has been taken                 -- dearest place to kill
//
// The library provides the hooks and the plumbing; what to do with them belongs to
// whoever is doing the study. Nothing here knows about a detector.
//
// EVERY OBSERVER STARTS OFF. main registers what a build can do; the macro says
// what this run does:
//
//   /observer/list
//   /observer/run/enable   <name>
//   /observer/event/enable <name>
//   /observer/stack/enable <name>
//   /observer/track/enable <name>
//   /observer/step/enable  <name>
//
// The default matters because an observer may change the physics -- one that kills
// tracks certainly does -- and a run must not have that happen because somebody
// linked it in.
class Observer {
public:
  // The name is what a macro addresses. Keep it a short lowercase token.
  explicit Observer(std::string name) : fName(std::move(name)) {}
  virtual ~Observer() = default;

  const std::string & GetName() const { return fName; }

  bool IsEnabled() const { return fEnabled; }
  void SetEnabled(bool on) { fEnabled = on; }

  // What this observer is for, printed by /observer/list. Optional.
  virtual std::string Describe() const { return {}; }

  // RootManager owns the run's output file: ask it for a tree (AddTree) or hand it
  // an object to write (Add) rather than opening a file. One file per run then
  // holds everything, and no observer has to reason about which ROOT directory is
  // current. Both are optional.
  virtual void BeginOfRun(const G4Run *, RootManager *) {}
  virtual void EndOfRun(const G4Run *, RootManager *) {}

private:
  std::string fName;
  bool fEnabled = false;
};
