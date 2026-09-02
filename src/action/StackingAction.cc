#include "StackingObserver.hh"
#include "StackingAction.hh"

StackingAction::StackingAction(std::vector<StackingObserver *> observers)
  : fObservers(std::move(observers))
{
}

G4ClassificationOfNewTrack StackingAction::ClassifyNewTrack(const G4Track * track)
{
  // The FIRST observer to answer decides, and one that abstains does not count as
  // an answer. So an observer may speak only about what it cares about, and the
  // order observers were registered in is the order of precedence -- explicit,
  // rather than the last one silently overriding the rest.
  for (StackingObserver * observer : fObservers) {
    if (!observer->IsEnabled()) continue;
    G4ClassificationOfNewTrack classification = fUrgent;
    if (observer->Classify(track, classification)) return classification;
  }
  return fUrgent;
}

void StackingAction::NewStage()
{
  for (StackingObserver * observer : fObservers)
    if (observer->IsEnabled()) observer->NewStage();
}

void StackingAction::PrepareNewEvent()
{
  for (StackingObserver * observer : fObservers)
    if (observer->IsEnabled()) observer->PrepareNewEvent();
}
