#include <utility>

#include "TParameter.h"
#include "TTree.h"

#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4Gamma.hh"
#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4Run.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VSolid.hh"
#include "G4ios.hh"
#include "RockGammaSteppingObserver.hh"
#include "RootManager.hh"
#include "ObserverRegistry.hh"

RockGammaSteppingObserver::RockGammaSteppingObserver(std::set<std::string> fromVolumes, std::set<std::string> toVolumes)
  : SteppingObserver("rockgamma"),
    fFrom(std::move(fromVolumes)),
    fTo(std::move(toVolumes))
{
}

RockGammaSteppingObserver::~RockGammaSteppingObserver() = default;

namespace {
// Announces itself, so main names nothing and adding an observer is adding a file.
// The macro turns it on with /observer/enable rockgamma.
const bool registered =
    ObserverRegistry<SteppingObserver>::Add("rockgamma",
                                       [] { return std::make_shared<RockGammaSteppingObserver>(); });
} // namespace

void RockGammaSteppingObserver::BeginOfRun(const G4Run *, RootManager * root)
{
  fTree = nullptr;
  fEscapes = 0;
  if (!root) return;

  // Asked for, not opened: the tree lands in the run's output file next to the
  // Event tree, and RootManager writes and closes it.
  fTree = root->AddTree("Escape", "gammas leaving the rock");
  if (!fTree) return;

  fTree->Branch("energy", &fEnergy, "energy/F");
  fTree->Branch("x", &fX, "x/F");
  fTree->Branch("y", &fY, "y/F");
  fTree->Branch("z", &fZ, "z/F");
  fTree->Branch("ux", &fUx, "ux/F");
  fTree->Branch("uy", &fUy, "uy/F");
  fTree->Branch("uz", &fUz, "uz/F");
  fTree->Branch("time", &fTime, "time/F");
  fTree->Branch("event", &fEventId, "event/I");
}

void RockGammaSteppingObserver::Step(const G4Step * step)
{
  if (!fTree) return;

  G4Track * track = step->GetTrack();
  if (track->GetDefinition() != G4Gamma::Definition()) return;

  const G4VPhysicalVolume * pre = step->GetPreStepPoint()->GetPhysicalVolume();
  const G4VPhysicalVolume * post = step->GetPostStepPoint()->GetPhysicalVolume();
  if (!pre || !post || pre == post) return;

  if (fFrom.find(pre->GetName()) == fFrom.end()) return;
  if (fTo.find(post->GetName()) == fTo.end()) return;

  // The post-step point: the crossing itself, which is where the flux is defined.
  const G4StepPoint * at = step->GetPostStepPoint();
  const G4ThreeVector & position = at->GetPosition();
  const G4ThreeVector & direction = at->GetMomentumDirection();

  fEnergy = (float)at->GetKineticEnergy();
  fX = (float)position.x();
  fY = (float)position.y();
  fZ = (float)position.z();
  fUx = (float)direction.x();
  fUy = (float)direction.y();
  fUz = (float)direction.z();
  fTime = (float)at->GetGlobalTime();

  const G4Event * event = G4EventManager::GetEventManager()->GetConstCurrentEvent();
  fEventId = event ? event->GetEventID() : -1;

  // No cd() anywhere: TTree::Fill goes to the tree's own file, and the tree was
  // made in the right one by RootManager.
  fTree->Fill();
  fEscapes++;

  // Counted, so it has no further business here: step two starts a fresh gamma on
  // this surface. Leaving it alive would double-count the very background the two
  // steps are meant to compute once.
  track->SetTrackStatus(fStopAndKill);
}

void RockGammaSteppingObserver::EndOfRun(const G4Run * run, RootManager * root)
{
  if (!fTree || !root) return;

  // What the absolute normalisation needs, handed over so it is written into the
  // same file as the crossings and cannot be separated from them: how many decays
  // were simulated, and the volume they were sampled in.
  const long long nEvents = run ? run->GetNumberOfEvent() : 0;
  root->Add(new TParameter<Long64_t>("decays", nEvents));

  // The rock is the shell MINUS the hall inside it. GetCubicVolume() on the Rock
  // solid alone returns the grown shape including the hall, which would overstate
  // the sampling volume by the whole cavern -- a factor of four here. Both are
  // Monte Carlo estimates for boolean solids, so this carries their uncertainty.
  double shell = -1., hall = -1.;
  if (auto * store = G4LogicalVolumeStore::GetInstance()) {
    for (auto * lv : *store) {
      if (!lv || !lv->GetSolid()) continue;
      if (lv->GetName() == "Rock") shell = lv->GetSolid()->GetCubicVolume();
      if (lv->GetName() == "Hall") hall = lv->GetSolid()->GetCubicVolume();
    }
  }
  const double rockVolume = (shell > 0. && hall > 0.) ? shell - hall : -1.;
  root->Add(new TParameter<double>("rock_volume_mm3", rockVolume));

  G4cout << "RockGammaSteppingObserver: " << fEscapes << " gammas left the rock in " << nEvents << " decays"
         << G4endl;

  fTree = nullptr; // the file owns it and is about to close
}
