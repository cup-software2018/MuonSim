#include <sstream>

#include "AbsVertexGen.hh"
#include "G4Event.hh"
#include "G4Navigator.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include "G4TransportationManager.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VSolid.hh"

void AbsVertexGen::CheckInsideWorld(const G4ThreeVector & position, const std::string & who) const
{
  const G4VSolid * worldSolid = nullptr;
  if (auto * transportMgr = G4TransportationManager::GetTransportationManager()) {
    if (auto * navigator = transportMgr->GetNavigatorForTracking()) {
      if (auto * world = navigator->GetWorldVolume())
        worldSolid = world->GetLogicalVolume()->GetSolid(); // world is unrotated at the origin
    }
  }
  if (worldSolid == nullptr || worldSolid->Inside(position) == kInside) return;

  std::ostringstream msg;
  msg << "primary vertex position is outside the world volume: (" << position.x() / mm << ", "
      << position.y() / mm << ", " << position.z() / mm << ") mm"
      << "\n  generator: " << who;
  if (!fDescription.empty()) msg << "\n  from: " << fDescription;
  G4Exception("AbsVertexGen::CheckInsideWorld", "GEN620", FatalException, msg.str().c_str());
}

void AbsVertexGen::GenerateVertex(G4Event * event) const
{
  if (event == nullptr || !fPosGen) return;

  PosDir where = fPosGen->Generate();

  // A surface sample sits exactly on a boundary, where Inside() reports
  // kSurface; step a little along the emission direction so the vertex is
  // unambiguously inside. This is also what lets "onvolume in World" work.
  if (where.hasDirection) where.position += 1. * mm * where.direction;

  CheckInsideWorld(where.position, fPosGen->GetKey());

  auto * vertex = new G4PrimaryVertex(where.position, fTime);
  EmitParticles(vertex, where);
  event->AddPrimaryVertex(vertex);
}
