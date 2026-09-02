#include <iomanip>
#include <iostream>

#include "MCPrimary.hh"

ClassImp(MCPrimary)

MCPrimary::MCPrimary()
  : TObject()
{
}

MCPrimary::MCPrimary(const MCPrimary & prim)
  : TObject(),
    fPDGCode(prim.GetPDGCode()),
    fTrackId(prim.GetTrackId()),
    fKineticEnergy(prim.GetKineticEnergy()),
    fT0(prim.GetT0())
{
  prim.GetVertex(fVx, fVy, fVz);
  prim.GetMomentum(fPx, fPy, fPz);
}

MCPrimary::~MCPrimary() = default;

// All of it: MCPrimaryData reuses these objects, so a member missed here would
// carry over from the previous event.
void MCPrimary::Clear(Option_t *)
{
  fPDGCode = 0;
  fTrackId = 0;
  fVx = fVy = fVz = 0;
  fPx = fPy = fPz = 0;
  fKineticEnergy = 0;
  fT0 = 0;
}

void MCPrimary::Print(Option_t * opt) const
{
  std::cout << "          PDG code : " << fPDGCode << std::endl;
  std::cout << "       Global Time : " << fT0 << " [ns]" << std::endl;
  std::cout << "    Kinetic Energy : " << fKineticEnergy << " [MeV]" << std::endl;
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "          Momentum : " << std::setw(7) << fPx << " " << std::setw(7) << fPy << " "
            << std::setw(7) << fPz << " [MeV]\n";
  std::cout << "            Vertex : " << std::setw(7) << fVx << " " << std::setw(7) << fVy << " "
            << std::setw(7) << fVz << " [mm]\n";
}
