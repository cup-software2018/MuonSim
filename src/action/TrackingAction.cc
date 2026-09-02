#include "G4Track.hh"
#include "G4TrackingManager.hh"
#include "RootManager.hh"
#include "TrackingObserver.hh"
#include "TrackingAction.hh"
#include "Trajectory.hh"

TrackingAction::TrackingAction(RootManager * r, std::vector<TrackingObserver *> observers)
  : fRootManager(r),
    fObservers(std::move(observers))
{
}

void TrackingAction::PreUserTrackingAction(const G4Track * aTrack)
{
  // Trajectory must be set before any early return — ProcessOneTrack accesses it after Pre.
  fpTrackingManager->SetTrajectory(new Trajectory(aTrack));

  // Delta electrons from muon ionization in air have no detector relevance — kill immediately.
  const G4VProcess * proc = aTrack->GetCreatorProcess();
  const G4LogicalVolume * lv = aTrack->GetLogicalVolumeAtVertex();
  if (proc && proc->GetProcessName() == "muIoni" &&
      aTrack->GetParticleDefinition()->GetParticleName() == "e-" && lv &&
      lv->GetMaterial()->GetName() == "G4_AIR") {
    const_cast<G4Track *>(aTrack)->SetTrackStatus(fKillTrackAndSecondaries);
  }

  // Create the MCTrack here, BEFORE the track takes its first step.
  //
  // RootManager::RecordStep can only attach a step to an MCTrack that already
  // exists, so registering the track any later silently loses the steps taken
  // before that point. This used to run in PostUserTrackingAction, which looks
  // equivalent because every track reaches it -- but Geant4 also calls Post every
  // time a track is SUSPENDED, and the optical processes suspend a track the moment
  // it starts making Cherenkov light. So a muon was registered only on entering the
  // radiator, and its whole path upstream of that went unrecorded, while a track
  // that never made light got no steps at all -- a failure that hides itself when
  // everything of interest happens to be inside the radiator.
  //
  // RecordTrack is idempotent: it looks the track up first and only fills in a new
  // one, so the repeated calls at each suspension do nothing.
  fRootManager->RecordTrack(aTrack);

  // Non-const, so an observer may kill the track here: the const the Geant4 hook
  // hands over is about not rewriting history, not about the decision to stop.
  for (TrackingObserver * observer : fObservers)
    if (observer->IsEnabled()) observer->PreTracking(const_cast<G4Track *>(aTrack));
}

void TrackingAction::PostUserTrackingAction(const G4Track * aTrack)
{
  for (TrackingObserver * observer : fObservers)
    if (observer->IsEnabled()) observer->PostTracking(aTrack);

  Trajectory * trajectory = (Trajectory *)fpTrackingManager->GimmeTrajectory();
  if (trajectory) trajectory->SetDrawTrajectory(true);
}