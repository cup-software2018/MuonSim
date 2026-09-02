#include <cstdio>

#include "MCScintStep.hh"

ClassImp(MCScintStep)

MCScintStep::MCScintStep()
  : MCStep()
{
}

MCScintStep::MCScintStep(const MCScintStep & step)
  : MCStep(step),
    fPdgCode(step.GetPdgCode()),
    fTrackId(step.GetTrackId()),
    fEnergyDepositVisible(step.GetEnergyDepositVisible())
{
}

void MCScintStep::Clear(Option_t * opt)
{
  MCStep::Clear(opt);
  fPdgCode = 0;
  fTrackId = 0;
  fEnergyDepositVisible = 0;
}

void MCScintStep::Print(Option_t *) const
{
  float x, y, z;
  GetStepPoint(x, y, z);
  std::printf("  MCScintStep pdg %6d trk %5d  edep %8.4f MeV  visible %8.4f MeV  t %10.3f ns"
              "  (%8.1f, %8.1f, %8.1f) mm  len %7.3f mm  %s\n",
              fPdgCode, fTrackId, GetEnergyDeposit(), fEnergyDepositVisible, GetGlobalTime(), x, y,
              z, GetStepLength(), GetProcessName().c_str());
}
