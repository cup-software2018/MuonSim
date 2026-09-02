#include "G4EmStandardPhysics_option4.hh"
#include "G4OpticalPhysics.hh"
#include "G4SystemOfUnits.hh"
#include "PMTFastSimPhysics.hh"
#include "UndergroundPhysicsList.hh"

UndergroundPhysicsList::UndergroundPhysicsList()
  : Shielding()
{
  // Replace default EM physics with Option 4
  // Provides high precision for low-energy EM interactions
  ReplacePhysics(new G4EmStandardPhysics_option4());

  // Add Optical Physics
  // Required to simulate Cherenkov and scintillation light
  G4OpticalPhysics * opticalPhysics = new G4OpticalPhysics();
  RegisterPhysics(opticalPhysics);

  RegisterPhysics(new PMTFastSimPhysics());
}

UndergroundPhysicsList::~UndergroundPhysicsList()
{
  // Destructor
}

void UndergroundPhysicsList::SetCuts()
{
  // Set the default cut value
  SetDefaultCutValue(1.0 * mm);

  // Apply cuts
  Shielding::SetCuts();
}