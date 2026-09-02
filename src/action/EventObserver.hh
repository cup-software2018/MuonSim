#pragma once

#include "Observer.hh"

class G4Event;

// Sees each event, before anything in it is tracked and after everything is.
//
// This is where a decision about the event AS A WHOLE goes: whether the energy
// deposits in it amount to a trigger, whether two hits fall close enough together
// to be one pulse, what a per-event summary should say. A SteppingObserver sees
// pieces and would have to accumulate them itself; here the event is finished.
//
// EndOfEvent runs BEFORE RootManager writes the event out, so an observer may still
// add to what is written -- and must not assume the output already exists.
//
// The event is const: by EndOfEvent the physics has happened and rewriting it would
// make the output disagree with what was simulated. To CHANGE what an event
// contains, act earlier -- StackingObserver decides what gets tracked at all.
class EventObserver : public Observer {
public:
  using Observer::Observer;

  // Neither is pure: an observer that only cares about the finished event should
  // not have to write an empty BeginOfEvent to say so.
  virtual void BeginOfEvent(const G4Event *) {}
  virtual void EndOfEvent(const G4Event *) {}
};
