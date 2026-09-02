#pragma once

#include "Observer.hh"

// An observer that only wants the run.
//
// It adds NOTHING to Observer, and that is deliberate: BeginOfRun and EndOfRun are
// on the base already, because every observer needs somewhere to open and close
// whatever it writes. A second pair of run hooks here would fire at the same moment
// as those and there would be no way to tell which one a reader was looking at.
//
// What this family adds is a way IN. Registration and /observer/<family>/enable are
// per family, so without one of these an observer wanting only run hooks would have
// to join stack, track or step and implement that family's pure virtual -- writing
// an empty Step() to get at EndOfRun. This is the family for work that belongs to
// the run itself: writing a normalisation constant, a configuration record, a
// summary gathered from somewhere else.
class RunObserver : public Observer {
public:
  using Observer::Observer;
};
