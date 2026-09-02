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
    G4Exception("CosmicMuonGen", "GEN630", JustWarning,
                ("Cannot open muon flux file: " + fluxFile +
                 " -- falling back to 1 GeV mu- straight down")
                    .c_str());
    delete f;
    return;
  }

  fHistFlux = static_cast<TH3D *>(f->Get("h_dJdEdTdP"));
  if (fHistFlux == nullptr) {
    G4Exception("CosmicMuonGen", "GEN631", JustWarning,
                ("Missing histogram h_dJdEdTdP (TH3D: x=energy[GeV], y=theta[deg], "
                 "z=phi[deg]) in " + fluxFile + " -- falling back to 1 GeV mu- straight down")
                    .c_str());
  }
  else {
    fHistFlux->SetDirectory(nullptr); // detach so it survives the file closing
    BuildCDF();
  }

  f->Close();
  delete f;
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
        // Foot-point plane: the world's widest horizontal cross-section, clamped
        // into its z range. For a sphere or an upper hemisphere centred on the
        // origin that is z = 0; for a world that does not straddle z = 0 it is the
        // nearest face. Using the widest section is what makes the disk cover every
        // vertical line through the world.
        if (!fHasPlaneZ) {
          fPlaneZ = std::min(std::max(0., pMin.z()), pMax.z());
          fHasPlaneZ = true;
        }
      }
    }
  }
  return fWorldRadius;
}

void CosmicMuonGen::GenerateVertex(G4Event * event) const
{
  if (event == nullptr || fMuon == nullptr) return;

  const G4double worldRadius = GetWorldRadius();
  if (worldRadius <= 0.) return;

  G4ThreeVector position;
  G4ThreeVector direction;
  G4double energy = 1. * GeV;

  if (fHistFlux == nullptr) { // no flux table: straight down from near the top
    position = G4ThreeVector(0., 0., worldRadius * 0.9);
    direction = G4ThreeVector(0., 0., -1.);
  }
  else {
    const G4double surfaceRadius = GetSurfaceRadius();
    const G4ThreeVector & centre = GetSurfaceCentre();

    // A point on the hemisphere's FLAT BOTTOM, uniformly: whatever direction a
    // muon arrives with, it lands uniformly over a horizontal plane, so that is
    // where the aim point is drawn.
    const G4double r = surfaceRadius * std::sqrt(G4UniformRand());
    const G4double alpha = twopi * G4UniformRand();
    const G4ThreeVector onFloor =
        centre + G4ThreeVector(r * std::cos(alpha), r * std::sin(alpha), 0.);

    // The direction it arrives with.
    G4double theta = 0., phi = 0.;
    SampleFlux(energy, theta, phi);

    const G4double sinTheta = std::sin(theta);
    const G4double cosTheta = std::cos(theta);
    const G4double sinPhi = std::sin(phi);
    const G4double cosPhi = std::cos(phi);

    direction = G4ThreeVector(-sinTheta * cosPhi, -sinTheta * sinPhi, -cosTheta);

    // Back up that direction until the sphere: with w = onFloor - centre, which is
    // horizontal and |w| = r, solving |w - t*dir|^2 = R^2 gives
    //   t = (w.dir) + sqrt((w.dir)^2 + R^2 - r^2)
    // and the positive root is the one upstream of the aim point. R^2 >= r^2 keeps
    // it real, and t > 0 with a downward dir puts the start point above the floor,
    // so it is always ON THE DOME -- nothing to reject.
    const G4ThreeVector w = onFloor - centre;
    const G4double wd = w.dot(direction);
    const G4double t = wd + std::sqrt(std::max(0., wd * wd + surfaceRadius * surfaceRadius - r * r));
    position = onFloor - t * direction;

    fSurfaceTries++;
    fSurfaceAccepted++;

    // A step along the muon before the vertex is placed. It matters only when the
    // surface grazes a geometry boundary -- which the fallback surface does, being
    // the world's own extent: landing exactly on it makes Inside() report kSurface
    // and the out-of-world guard reject the vertex.
    position += 1. * mm * direction;
  }

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
