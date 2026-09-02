#include "G4Colour.hh"
#include "G4OpticalPhoton.hh"
#include "G4ParticleDefinition.hh"
#include "G4Polyline.hh"
#include "G4Polymarker.hh"
#include "G4ThreeVector.hh"
#include "G4Trajectory.hh"
#include "G4VVisManager.hh"
#include "G4VisAttributes.hh"
#include "Trajectory.hh"

G4ThreadLocal G4Allocator<Trajectory> * TrajectoryAllocator = nullptr;

Trajectory::Trajectory()
  : G4Trajectory(),
    fDrawit(false),
    fForceNoDraw(false),
    fForceDraw(false)
{
  fParticleDefinition = nullptr;
}

Trajectory::Trajectory(const G4Track * aTrack)
  : G4Trajectory(aTrack),
    fDrawit(false)
{
  fParticleDefinition = aTrack->GetDefinition();
}

Trajectory::Trajectory(Trajectory & right)
  : G4Trajectory(right),
    fDrawit(right.fDrawit)
{
  fParticleDefinition = right.fParticleDefinition;
}

Trajectory::~Trajectory() {}

void Trajectory::DrawTrajectory() const
{
  if (!fForceDraw && (!fDrawit || fForceNoDraw)) return;

  G4VVisManager * pVVisManager = G4VVisManager::GetConcreteInstance();
  if (!pVVisManager) return;

  const G4double markerSize = 0.05;
  G4bool lineRequired = true;
  G4bool markersRequired = true;

  G4Polyline trajectoryLine;
  G4Polymarker stepPoints;
  G4Polymarker auxiliaryPoints;

  for (G4int i = 0; i < GetPointEntries(); ++i) {
    G4VTrajectoryPoint * aTrajectoryPoint = GetPoint(i);
    const std::vector<G4ThreeVector> * auxiliaries = aTrajectoryPoint->GetAuxiliaryPoints();
    if (auxiliaries) {
      for (size_t iAux = 0; iAux < auxiliaries->size(); ++iAux) {
        const G4ThreeVector pos((*auxiliaries)[iAux]);
        if (lineRequired) { trajectoryLine.push_back(pos); }
        if (markersRequired) { auxiliaryPoints.push_back(pos); }
      }
    }
    const G4ThreeVector pos(aTrajectoryPoint->GetPosition());
    if (lineRequired) { trajectoryLine.push_back(pos); }
    if (markersRequired) { stepPoints.push_back(pos); }
  }

  if (lineRequired) {
    G4Colour colour;

    if (fParticleDefinition == G4OpticalPhoton::OpticalPhotonDefinition()) {
      colour = G4Colour(0., 1., 0.);
    }
    else // All other particles are blue
      colour = G4Colour(0., 0., 1.);

    G4VisAttributes trajectoryLineAttribs(colour);
    trajectoryLine.SetVisAttributes(&trajectoryLineAttribs);
    pVVisManager->Draw(trajectoryLine);
  }
  if (markersRequired) {
    auxiliaryPoints.SetMarkerType(G4Polymarker::squares);
    auxiliaryPoints.SetScreenSize(markerSize);
    auxiliaryPoints.SetFillStyle(G4VMarker::filled);
    G4VisAttributes auxiliaryPointsAttribs(G4Colour(0., 1., 1.)); // Magenta
    auxiliaryPoints.SetVisAttributes(&auxiliaryPointsAttribs);
    pVVisManager->Draw(auxiliaryPoints);

    stepPoints.SetMarkerType(G4Polymarker::circles);
    stepPoints.SetScreenSize(markerSize);
    stepPoints.SetFillStyle(G4VMarker::filled);
    G4VisAttributes stepPointsAttribs(G4Colour(1., 1., 0.)); // Yellow
    stepPoints.SetVisAttributes(&stepPointsAttribs);
    pVVisManager->Draw(stepPoints);
  }
}