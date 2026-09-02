#include "ActionInitialization.hh"
#include "EventAction.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "StackingAction.hh"
#include "SteppingAction.hh"
#include "TrackingAction.hh"



ActionInitialization::ActionInitialization(
    const G4String & rootFilename,
    std::vector<std::shared_ptr<SteppingObserver>> stepObservers,
    std::vector<std::shared_ptr<TrackingObserver>> trackObservers,
    std::vector<std::shared_ptr<StackingObserver>> stackObservers,
    std::vector<std::shared_ptr<EventObserver>> eventObservers,
    std::vector<std::shared_ptr<RunObserver>> runObservers)
  : fRootFilename(rootFilename),
    fSteppingObservers(std::move(stepObservers)),
    fTrackingObservers(std::move(trackObservers)),
    fStackingObservers(std::move(stackObservers)),
    fEventObservers(std::move(eventObservers)),
    fRunObservers(std::move(runObservers))
{
  // Base pointers for the messenger: enabling and listing need only the name and
  // the flag, which live on Observer.
  auto base = [](auto & owned) {
    std::vector<Observer *> raw;
    raw.reserve(owned.size());
    for (const auto & observer : owned) raw.push_back(observer.get());
    return raw;
  };
  // Outermost first, which is the order /observer/list prints and the order a run
  // actually meets them.
  fObserverMessenger = std::make_unique<ObserverMessenger>(
      std::vector<ObserverMessenger::FamilyInput>{{"run", base(fRunObservers)},
                                                 {"event", base(fEventObservers)},
                                                 {"stack", base(fStackingObservers)},
                                                 {"track", base(fTrackingObservers)},
                                                 {"step", base(fSteppingObservers)}});

  fRootManager = new RootManager();
  fRootManager->SetRootFilename(fRootFilename);
}


namespace {

// The actions take raw pointers: they use the observers, they do not own them.
template <class T>
std::vector<T *> Raw(const std::vector<std::shared_ptr<T>> & owned)
{
  std::vector<T *> raw;
  raw.reserve(owned.size());
  for (const auto & observer : owned) raw.push_back(observer.get());
  return raw;
}

} // namespace

void ActionInitialization::BuildForMaster() const
{
  SetUserAction(new RunAction(fRootManager, AllObservers()));
}

// Every family gets BeginOfRun and EndOfRun, so RunAction is handed all of them as
// the common base -- including the run family, whose observers want nothing else.
std::vector<Observer *> ActionInitialization::AllObservers() const
{
  std::vector<Observer *> all;
  for (const auto & o : fRunObservers) all.push_back(o.get());
  for (const auto & o : fEventObservers) all.push_back(o.get());
  for (const auto & o : fStackingObservers) all.push_back(o.get());
  for (const auto & o : fTrackingObservers) all.push_back(o.get());
  for (const auto & o : fSteppingObservers) all.push_back(o.get());
  return all;
}


void ActionInitialization::Build() const
{
  // The file is NOT opened here. RootManager::BeginOfRun does it, at /run/beamOn.
  // Opening it here as well left two TFile objects on one path -- the second
  // "recreate" truncating the file the first still pointed at -- and the first was
  // never closed. With a run of zero events the run actions never fire, so that
  // stale file stayed open to the end and ROOT aborted tearing its dictionaries
  // down at exit.
  SetUserAction(new PrimaryGeneratorAction());

  auto * runAction = new RunAction(fRootManager, AllObservers());
  SetUserAction(runAction);

  auto * eventAction = new EventAction(fRootManager, Raw(fEventObservers));
  SetUserAction(eventAction);
  SetUserAction(new TrackingAction(fRootManager, Raw(fTrackingObservers)));
  SetUserAction(new SteppingAction(fRootManager, fRegionTagger, fDetectorIDTagger,
                                   Raw(fSteppingObservers)));
  SetUserAction(new StackingAction(Raw(fStackingObservers)));
}

