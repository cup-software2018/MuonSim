#include "MCStep.hh"

ClassImp(MCStep)

MCStep::MCStep()
  : TObject()
{
}

MCStep::MCStep(const MCStep & step)
  : TObject(),
    fStepLength(step.GetStepLength()),
    fEnergyDeposit(step.GetEnergyDeposit()),
    fEnergyDepositNonIonizing(step.GetEnergyDepositNonIonizing()),
    fKineticEnergy(step.GetKineticEnergy()),
    fGlobalTime(step.GetGlobalTime()),
    fLocalTime(step.GetLocalTime()),
    fVolumeName(step.GetVolumeName()),
    fProcessName(step.GetProcessName()),
    fDetectorID(step.GetDetectorID())
{
  step.GetStepPoint(fX, fY, fZ);
}

void MCStep::Clear(Option_t *)
{
  fStepLength = 0;
  fEnergyDeposit = 0;
  fEnergyDepositNonIonizing = 0;
  fKineticEnergy = 0;
  fGlobalTime = 0;
  fLocalTime = 0;
  fX = fY = fZ = 0;
  fVolumeName.clear();
  fProcessName.clear();
  fDetectorID = -1;
}
