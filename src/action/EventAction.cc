#include "EventAction.hh"
#include "EventObserver.hh"
#include "G4Event.hh"
#include "RootManager.hh"


EventAction::EventAction(RootManager * rootManager, std::vector<EventObserver *> observers)
  : fRootManager(rootManager),
    fObservers(std::move(observers))
{
}


void EventAction::BeginOfEventAction(const G4Event * event)
{
  fRootManager->BeginOfEvent(event);

  for (EventObserver * observer : fObservers)
    if (observer->IsEnabled()) observer->BeginOfEvent(event);
}


void EventAction::EndOfEventAction(const G4Event * event)
{
  // Observers BEFORE RootManager, so one that wants to add to the output still can:
  // RootManager::EndOfEvent is what fills the trees and clears the containers, and
  // after it there is nothing left to add to.
  for (EventObserver * observer : fObservers)
    if (observer->IsEnabled()) observer->EndOfEvent(event);

  fRootManager->EndOfEvent(event);
}
