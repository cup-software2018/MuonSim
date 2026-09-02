#include "G4Step.hh"
#include "G4SteppingManager.hh"
#include "G4VProcess.hh"
#include "G4Track.hh"
#include "RootManager.hh"
#include "SteppingObserver.hh"
#include "SteppingAction.hh"

SteppingAction::SteppingAction(RootManager * rootManager, RegionTagger tagger,
                               DetectorIDTagger idTagger,
                               std::vector<SteppingObserver *> observers)
  : fRootManager(rootManager),
    fRegionTagger(std::move(tagger)),
    fDetectorIDTagger(std::move(idTagger)),
    fObservers(std::move(observers))
{
}

void SteppingAction::UserSteppingAction(const G4Step * step)
{
  // The process that actually ended the step -- NOT
  // fpSteppingManager->GetfCurrentProcess(), which was used here before and is a
  // different thing: it holds whichever process the stepping manager invoked last
  // in its along-step/post-step loop. With optical physics registered that is
  // Scintillation, so every step in the output claimed to be Scintillation,
  // whatever had really happened. GetProcessDefinedStep is the one that limited
  // the step, and it is Transportation for a boundary crossing.
  const G4VProcess * proc = step->GetPostStepPoint()->GetProcessDefinedStep();

  const G4int detectorID = fDetectorIDTagger ? fDetectorIDTagger(step) : -1;
  fRootManager->RecordStep(step, proc, detectorID);

  if (fRegionTagger) {
    const G4int bits = fRegionTagger(step);
    if (bits != 0) fRootManager->AddRegion(step->GetTrack()->GetTrackID(), bits);
  }

  // Last, so that everything above has already recorded this step: an observer is
  // allowed to kill the track, and a step that ends a track still belongs in the
  // output. Only the ones the macro asked for -- see /observer/enable.
  for (SteppingObserver * observer : fObservers)
    if (observer->IsEnabled()) observer->Step(step);
}
