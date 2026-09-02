#include <cstdio>

#include "MCScint.hh"

ClassImp(MCScint)

MCScint::MCScint()
  : TObject()
{
}

MCScint::MCScint(const MCScint & scint)
  : TObject(),
    fId(scint.GetId()),
    fEnergyDeposit(scint.GetEnergyDeposit()),
    fEnergyDepositVisible(scint.GetEnergyDepositVisible()),
    fTime(scint.GetTime()),
    fTimeLast(scint.GetTimeLast()),
    fNTrack(scint.GetNTrack()),
    fPdgDominant(scint.GetPdgDominant())
{
  scint.GetPosition(fX, fY, fZ);
}

void MCScint::Clear(Option_t *)
{
  fId = -1;
  fEnergyDeposit = 0;
  fEnergyDepositVisible = 0;
  fTime = 0;
  fTimeLast = 0;
  fX = fY = fZ = 0;
  fNTrack = 0;
  fPdgDominant = 0;
  fSteps.clear();
}

MCScintStep * MCScint::AddStep()
{
  fSteps.emplace_back();
  return &fSteps.back();
}

int MCScint::Compare(const TObject * object) const
{
  auto comp = static_cast<const MCScint *>(object);
  if (this->GetId() > comp->GetId()) return 1;
  if (this->GetId() < comp->GetId()) return -1;
  return 0;
}

void MCScint::Print(Option_t *) const
{
  std::printf("MCScint id %4d  edep %8.3f MeV  visible %8.3f MeV  t %10.3f .. %10.3f ns"
              "  first (%8.1f, %8.1f, %8.1f) mm  ntrack %3d  pdg %6d  nstep %d\n",
              fId, fEnergyDeposit, fEnergyDepositVisible, fTime, fTimeLast, fX, fY, fZ, fNTrack,
              fPdgDominant, (int)fSteps.size());
  for (const auto & step : fSteps)
    step.Print();
}
