#include <iomanip>
#include <iostream>

#include "MCTrack.hh"

ClassImp(MCTrack)

MCTrack::MCTrack()
  : TObject()
{
}

MCTrack::MCTrack(const MCTrack & trk)
  : TObject(trk),
    fPDGCode(trk.GetPDGCode()),
    fTrackId(trk.GetTrackId()),
    fParentId(trk.GetParentId()),
    fKineticEnergy(trk.GetKineticEnergy()),
    fGlobalTime(trk.GetGlobalTime()),
    fLocalTime(trk.GetLocalTime()),
    fProcessName(trk.GetProcessName()),
    fSteps(trk.fSteps)
{
  trk.GetVertex(fVx, fVy, fVz);
}

MCTrack::~MCTrack() = default;

MCStep * MCTrack::AddStep()
{
  fSteps.emplace_back();
  return &fSteps.back();
}

// Every member, not just the containers: with the array reusing objects, a field
// left alone here silently keeps the previous event's value.
void MCTrack::Clear(Option_t * opt)
{
  fPDGCode = 0;
  fTrackId = 0;
  fParentId = 0;
  fVx = fVy = fVz = 0;
  fDirX = fDirY = fDirZ = 0;
  fKineticEnergy = 0;
  fGlobalTime = 0;
  fLocalTime = 0;
  fRegionMask = 0;
  fProcessName.clear();
  fSteps.clear();
}

void MCTrack::Print(Option_t * opt) const
{
  std::cout << "+++++++++ TrackId = " << fTrackId << std::endl;
  std::cout << "         MotherId = " << fParentId << std::endl;
  std::cout << "     ProcessName  = " << fProcessName << std::endl;
  std::cout << std::fixed << std::setprecision(6);
  std::cout << "     Global Time  = " << fGlobalTime << " [ns]\n";
  std::cout << "   Kinetic Energy = " << fKineticEnergy << " [MeV]\n";
  std::cout << "      Region Mask = 0x" << std::hex << fRegionMask << std::dec << "\n";
  std::cout << std::endl;
}
