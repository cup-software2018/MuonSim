#include <algorithm>
#include <cmath>

#include "TFile.h"
#include "TH3D.h"

#include "CosmicMuonGen.hh"
#include "G4Event.hh"
#include "G4Navigator.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4PhysicalConstants.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include "G4TransportationManager.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VSolid.hh"
#include "Randomize.hh"

CosmicMuonGen::CosmicMuonGen(const std::string & fluxFile)
{
  fMuon = G4ParticleTable::GetParticleTable()->FindParticle("mu-");

  auto * f = TFile::Open(fluxFile.c_str());
  if (f == nullptr || f->IsZombie()) {
    delete f;
    G4Exception("CosmicMuonGen", "GEN631", FatalException,
                ("Cannot open muon flux file: " + fluxFile).c_str());
    return;
  }

  fHistFlux = static_cast<TH3D *>(f->Get("h_flux"));
  if (fHistFlux == nullptr) {
    f->Close();
    delete f;
    // Fatal, where this used to warn and fall back to 1 GeV straight down. That
    // fallback ran, filled an output file and answered a different question, which
    // is worse than not starting.
    G4Exception("CosmicMuonGen", "GEN631", FatalException,
                ("No histogram h_flux (TH3D: x=energy[GeV], y=theta[deg], z=phi[deg]) in " +
                 fluxFile + " -- this must be the SPHERE flux file")
                    .c_str());
    return;
  }

  fHistFlux->SetDirectory(nullptr); // detach so it survives the file closing
  fFluxIntegral = fHistFlux->Integral();
  BuildCDF();

  f->Close();
  delete f;

  // Printed because nothing in the file says whether it is the sphere or the plane
  // version, and the plane one is wrong here by 32%. Check this against the number
  // the flux note quotes for the sphere file.
  G4cout << "CosmicMuonGen: h_flux from " << fluxFile << ", integral " << fFluxIntegral
         << " /cm2/s = " << fFluxIntegral * 1e4 * 86400. << " /m2/day" << G4endl;
  G4cout << "CosmicMuonGen: this must be the SPHERE file -- the plane file has cos(theta)"
         << " folded in and would be wrong by 32%" << G4endl;
}

CosmicMuonGen::~CosmicMuonGen() { delete fHistFlux; }

const std::string & CosmicMuonGen::Key()
{
  static const std::string key = "cosmic";
  return key;
}

void CosmicMuonGen::BuildCDF()
{
  const G4int nx = fHistFlux->GetNbinsX();
  const G4int ny = fHistFlux->GetNbinsY();
  const G4int nz = fHistFlux->GetNbinsZ();

  fNBinsY = ny;
  fNBinsZ = nz;

  const G4int nbins = nx * ny * nz;
  fCDF.resize(nbins + 1);
  fCDF[0] = 0.;

  G4int k = 0;
  for (G4int ix = 1; ix <= nx; ix++)
    for (G4int iy = 1; iy <= ny; iy++)
      for (G4int iz = 1; iz <= nz; iz++, ++k)
        fCDF[k + 1] = fCDF[k] + fHistFlux->GetBinContent(ix, iy, iz);

  const G4double total = fCDF[nbins];
  if (total > 0.) {
    for (auto & v : fCDF)
      v /= total;
  }
}

void CosmicMuonGen::SampleFlux(G4double & energy, G4double & theta, G4double & phi) const
{
  const G4double r = G4UniformRand();

  // Find k with fCDF[k] <= r < fCDF[k+1], then pick uniformly inside that bin.
  G4int k = (G4int)(std::upper_bound(fCDF.begin(), fCDF.end(), r) - fCDF.begin()) - 1;
  const G4int nbins = (G4int)fCDF.size() - 1;
  if (k < 0) k = 0;
  if (k >= nbins) k = nbins - 1;

  const G4int ix = k / (fNBinsY * fNBinsZ) + 1;
  const G4int iy = (k % (fNBinsY * fNBinsZ)) / fNBinsZ + 1;
  const G4int iz = k % fNBinsZ + 1;

  const G4double energyGeV = fHistFlux->GetXaxis()->GetBinLowEdge(ix) +
                             G4UniformRand() * fHistFlux->GetXaxis()->GetBinWidth(ix);
  const G4double thetaDeg = fHistFlux->GetYaxis()->GetBinLowEdge(iy) +
                            G4UniformRand() * fHistFlux->GetYaxis()->GetBinWidth(iy);
  const G4double phiDeg = fHistFlux->GetZaxis()->GetBinLowEdge(iz) +
                          G4UniformRand() * fHistFlux->GetZaxis()->GetBinWidth(iz);

  energy = energyGeV * GeV;
  theta = thetaDeg * deg;
  phi = phiDeg * deg;
}

void CosmicMuonGen::SetSurface(const G4ThreeVector & centre, G4double radius)
{
  fSurfaceCentre = centre;
  fSurfaceRadius = radius;
  fHasSurface = true;
}

const G4ThreeVector & CosmicMuonGen::GetSurfaceCentre() const
{
  if (!fHasSurface) GetSurfaceRadius(); // fills in the fallback
  return fSurfaceCentre;
}

G4double CosmicMuonGen::GetSurfaceRadius() const
{
  if (fHasSurface) return fSurfaceRadius;

  // No surface given: fall back to one centred on the world origin and as big as
  // the world. That reproduces the acceptance a run had before the surface could
  // be set, at the cost of tying it to the geometry -- which is exactly what
  // setting it explicitly avoids.
  fSurfaceCentre = G4ThreeVector(0., 0., 0.);
  fSurfaceRadius = GetWorldRadius();
  fHasSurface = fSurfaceRadius > 0.;
  return fSurfaceRadius;
}

G4double CosmicMuonGen::GetWorldRadius() const
{
  if (fWorldRadius > 0.) return fWorldRadius;

  // Take it from the world solid rather than repeating the geometry's constant,
  // which would silently drift if the world were resized.
  if (auto * transportMgr = G4TransportationManager::GetTransportationManager()) {
    if (auto * navigator = transportMgr->GetNavigatorForTracking()) {
      if (auto * world = navigator->GetWorldVolume()) {
        G4ThreeVector pMin, pMax;
        world->GetLogicalVolume()->GetSolid()->BoundingLimits(pMin, pMax);
        fWorldRadius =
            std::max({pMax.x(), pMax.y(), pMax.z(), -pMin.x(), -pMin.y(), -pMin.z()});
      }
    }
  }
  return fWorldRadius;
}

G4double CosmicMuonGen::GetRateHz() const
{
  if (fFluxIntegral <= 0.) return 0.;
  // J is per cm2, so the projected area has to be in cm2 as well. pi R^2 and not
  // 2 pi R^2: a sphere's shadow is a disc, whatever direction it is seen from.
  const G4double radiusCm = GetSurfaceRadius() / cm;
  return fFluxIntegral * pi * radiusCm * radiusCm / second;
}

void CosmicMuonGen::GenerateVertex(G4Event * event) const
{
  if (event == nullptr || fMuon == nullptr || fHistFlux == nullptr) return;

  const G4double R = GetSurfaceRadius();
  const G4ThreeVector & C = GetSurfaceCentre();
  if (R <= 0.) return;

  // Where it came from, and so where it is going.
  G4double energy = 0., theta = 0., phi = 0.;
  SampleFlux(energy, theta, phi);

  const G4double sinTheta = std::sin(theta), cosTheta = std::cos(theta);
  const G4double sinPhi = std::sin(phi), cosPhi = std::cos(phi);
  const G4ThreeVector direction(-sinTheta * cosPhi, -sinTheta * sinPhi, -cosTheta);

  // The shadow disc: radius R through C, perpendicular to the direction. Two axes
  // spanning it, then a point uniform PER UNIT AREA on it -- the sqrt is what makes
  // it uniform instead of piling up at the centre.
  const G4ThreeVector e1 = direction.orthogonal().unit();
  const G4ThreeVector e2 = direction.cross(e1).unit();

  const G4double rr = R * std::sqrt(G4UniformRand());
  const G4double aa = twopi * G4UniformRand();
  const G4ThreeVector aim = C + rr * (std::cos(aa) * e1 + std::sin(aa) * e2);

  // Walk back up onto the sphere. |pos - C|^2 = rr^2 + (R^2 - rr^2) = R^2 exactly,
  // and the point is always upstream of the aim, so nothing is ever rejected and
  // there is no acceptance factor to carry.
  G4ThreeVector position = aim - std::sqrt(std::max(0., R * R - rr * rr)) * direction;

  // A step inwards. It matters when the sphere grazes a geometry boundary -- which
  // the fallback surface does, being the world's own extent: landing exactly on it
  // makes Inside() report kSurface and the out-of-world guard refuses the vertex.
  position += 1. * mm * direction;

  CheckInsideWorld(position, "CosmicMuon");

  // GetTime(), not 0: this override would otherwise ignore the pileup offset and
  // stack every piled-up muon at the same instant.
  auto * vertex = new G4PrimaryVertex(position, GetTime());
  auto * primary = new G4PrimaryParticle(fMuon);
  primary->SetMomentumDirection(direction);
  primary->SetKineticEnergy(energy);
  vertex->SetPrimary(primary);
  event->AddPrimaryVertex(vertex);
}
