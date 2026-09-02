#pragma once

#include "G4String.hh"
#include "G4UImessenger.hh"

class PrimaryGeneratorAction;
class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithoutParameter;
class G4UIcmdWithABool;
class G4UIcmdWithADoubleAndUnit;

// Registers the /gen/... commands and forwards them to PrimaryGeneratorAction.
//
// Deliberately thin: parsing a command line into generator objects lives in
// gen/VertexGenBuilder, and the vertex list lives in the action, so this class
// is the Geant4-UI adapter and nothing more.
class PrimaryGeneratorMessenger : public G4UImessenger {
public:
  explicit PrimaryGeneratorMessenger(PrimaryGeneratorAction * action);
  ~PrimaryGeneratorMessenger() override;

  void SetNewValue(G4UIcommand * command, G4String newValue) override;

private:
  PrimaryGeneratorAction * fAction;

  G4UIdirectory * fDir;
  G4UIcmdWithAString * fVertexCmd;
  G4UIcmdWithoutParameter * fClearCmd;
  G4UIcmdWithoutParameter * fListCmd;
  G4UIcmdWithABool * fYieldCmd;
  G4UIcmdWithADoubleAndUnit * fActivityCmd;
  G4UIcmdWithADoubleAndUnit * fWindowCmd;
};
