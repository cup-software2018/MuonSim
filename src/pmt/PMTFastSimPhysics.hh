#pragma once

#include "G4VPhysicsConstructor.hh"

// Registers G4FastSimulationManagerProcess for optical photons so that
// PMTOpticalModel (a G4VFastSimulationModel) is invoked when photons
// enter the PMT glass-body region.  Without this, the fast simulation
// model exists but is never triggered.
class PMTFastSimPhysics : public G4VPhysicsConstructor {
public:
  PMTFastSimPhysics()
    : G4VPhysicsConstructor("PMTFastSimPhysics")
  {
  }
  void ConstructParticle() override {}
  void ConstructProcess() override;
};
