#include "PMTFastSimPhysics.hh"
#include "G4FastSimulationManagerProcess.hh"
#include "G4ParticleDefinition.hh"
#include "G4ProcessManager.hh"

void PMTFastSimPhysics::ConstructProcess()
{
  // One shared instance for all particles, exactly as in LSCPhysicsList::AddParameterisation().
  // G4FastSimulationManagerProcess queries G4GlobalFastSimulationManager at runtime,
  // so a single instance works correctly for all particle types.
  auto * fsmp = new G4FastSimulationManagerProcess("PMTFastSim");

  auto theParticleIterator = GetParticleIterator();
  theParticleIterator->reset();
  while ((*theParticleIterator)()) {
    G4ParticleDefinition * particle = theParticleIterator->value();
    G4ProcessManager * pmanager = particle->GetProcessManager();
    // PostStepDoIt ordering 1: runs before G4OpBoundaryProcess (SetProcessOrderingToLast).
    pmanager->AddProcess(fsmp, -1, -1, 1);
  }
}
