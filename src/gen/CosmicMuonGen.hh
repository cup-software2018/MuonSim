#pragma once

#include <string>
#include <vector>

#include "G4ThreeVector.hh"
#include "AbsVertexGen.hh"
#include "G4Types.hh"

class G4Event;
class G4ParticleDefinition;
class TH3D;

// Cosmic muons entering the world from above.
//
// Per event:
//   1. sample (kinetic energy, zenith theta, azimuth phi) from the flux
//      histogram h_dJdEdTdP in a ROOT file, by inverse CDF over all its bins
//   2. pick a foot point uniformly on the horizontal disk at z = 0
//   3. trace that muon direction backwards from the foot point to the world
//      sphere -- that intersection is the start position
//
// Step 2 is uniform on a HORIZONTAL disk because muon flux tables are given per
// unit horizontal area (dJ/dE dtheta dphi), so weighting start points by
// horizontal projected area is what reproduces that normalisation. This is also
// why the position cannot come from an AbsPosGen: it is derived from the
// direction sampled in step 1.
//
// With no usable histogram it falls back to 1 GeV muons straight down, which
// keeps geometry-only runs (e.g. visualisation) working.
class CosmicMuonGen : public AbsVertexGen {
public:
  explicit CosmicMuonGen(const std::string & fluxFile);
  ~CosmicMuonGen() override;

  static const std::string & Key();
  const std::string & GetKey() const override { return Key(); }

  void GenerateVertex(G4Event * event) const override;

  bool HasFlux() const { return fHistFlux != nullptr; }

  // Radius of the sphere muons start on, and the z of the horizontal plane their
  // foot points are drawn on. Both are taken from the world solid's extent unless
  // set, so they cannot drift out of step with the geometry.
  // The virtual surface muons are launched from: a hemisphere of `radius`
  // centred on `centre`, dome upwards.
  //
  // How muons are placed: a point is drawn uniformly on the hemisphere's FLAT
  // BOTTOM -- a muon lands uniformly over a horizontal plane whatever direction it
  // came with -- the direction is drawn from the flux, and the start point is that
  // aim point backed up along the direction until it meets the sphere. That is
  // always on the dome, so every draw is used.
  //
  // The radius therefore has to enclose the apparatus and nothing more, the cost
  // going as its square, and the flat side is the plane the muons are spread over.
  void SetSurface(const G4ThreeVector & centre, G4double radius);
  const G4ThreeVector & GetSurfaceCentre() const;
  G4double GetSurfaceRadius() const;

  // How many draws it took, for the record: the absolute rate needs the accepted
  // fraction, since the effective area is that of the accepted set.
  G4long GetSurfaceTries() const { return fSurfaceTries; }
  G4long GetSurfaceAccepted() const { return fSurfaceAccepted; }

  void SetWorldRadius(G4double radius) { fWorldRadius = radius; }
  G4double GetWorldRadius() const;

  // Exposed for verification: energy in Geant4 units, angles in radians.
  void SampleFlux(G4double & energy, G4double & theta, G4double & phi) const;

protected:
  // Never reached: GenerateVertex is overridden because the position follows
  // from the sampled direction rather than from a position generator.
  void EmitParticles(G4PrimaryVertex *, const PosDir &) const override {}

private:

  mutable G4long fSurfaceTries = 0;
  mutable G4long fSurfaceAccepted = 0;

  void BuildCDF();

  G4ParticleDefinition * fMuon = nullptr;
  TH3D * fHistFlux = nullptr; // x: energy [GeV], y: theta [deg], z: phi [deg]

  std::vector<G4double> fCDF; // normalised cumulative bin sums, size = nx*ny*nz + 1
  G4int fNBinsY = 0;
  G4int fNBinsZ = 0;

  mutable G4double fWorldRadius = 0.; // 0 => derive from the world solid
  mutable G4ThreeVector fSurfaceCentre;
  mutable G4double fSurfaceRadius = 0.;
  mutable bool fHasSurface = false;
  mutable G4double fPlaneZ = 0.;
  mutable bool fHasPlaneZ = false;
};
