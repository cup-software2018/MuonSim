#pragma once

#include <string>
#include <vector>

#include "G4ThreeVector.hh"
#include "AbsVertexGen.hh"
#include "G4Types.hh"

class G4Event;
class G4ParticleDefinition;
class TH3D;

// Cosmic muons entering the world from above, launched from a SPHERE.
//
// Per event, following doc/generator.md and the flux note's recipe:
//   1. sample (kinetic energy, zenith theta, azimuth phi) from h_flux, by inverse
//      CDF over all its bins
//   2. draw a point uniformly on the sphere's SHADOW DISC -- the disc of radius R
//      through the centre, perpendicular to the muon's direction
//   3. walk back up the direction onto the sphere; that is the start point
//
// WHY A SPHERE AND NOT A HORIZONTAL DISC. The rate arriving from a direction is
// I(Omega) * A_perp(Omega), and A_perp is the surface's projected area. For a
// sphere that is pi R^2 from every direction -- a ball's shadow is the same circle
// whatever the angle -- so no cos(theta) enters, and the sphere file is the one
// whose bins already have no cos(theta) folded in.
//
// A horizontal disc has A_perp = A cos(theta) instead, which is a different file,
// and worse: it LEAKS. A muon drifts sideways as it descends, so it can hit the
// apparatus and cross the disc plane outside the disc, never being generated at
// all. Measured for the water tank, a disc at 9500 mm loses 12.9% of the muons
// that hit it, preferentially at large zenith angle, so the inclination
// distribution is skewed as well as the rate. A closed surface that ENCLOSES the
// apparatus cannot leak: anything reaching inside came in through it. That is a
// property of the shape, so the radius is then free -- and the answer must not
// depend on it, which is a test worth running.
//
// The construction rejects nothing, so there is no acceptance factor: |pos - C| is
// exactly R for every draw, and the start point is always upstream of the aim.
//
// THE FILE MUST BE THE SPHERE FILE. The plane file's bins carry cos(theta), and
// using it here would be wrong by the ratio of the two integrals -- 32% -- with
// nothing to notice it. Nothing in the file says which it is, so this class states
// the requirement and prints the integral it found; check it against the number the
// flux note quotes.
class CosmicMuonGen : public AbsVertexGen {
public:
  explicit CosmicMuonGen(const std::string & fluxFile);
  ~CosmicMuonGen() override;

  static const std::string & Key();
  const std::string & GetKey() const override { return Key(); }

  void GenerateVertex(G4Event * event) const override;

  bool HasFlux() const { return fHistFlux != nullptr; }

  // The launch sphere: centre and radius. Taken from the world solid's extent
  // unless set, so it cannot drift out of step with the geometry.
  //
  // It only has to ENCLOSE whatever response is being asked about. It does not
  // have to be tight, the answer does not depend on it, and the cost goes as its
  // square -- a bigger sphere makes more muons and wastes more of them.
  void SetSurface(const G4ThreeVector & centre, G4double radius);
  const G4ThreeVector & GetSurfaceCentre() const;
  G4double GetSurfaceRadius() const;

  // The flux integral read from the histogram, in cm^-2 s^-1. This is the
  // omnidirectional flux, and it is read rather than assumed.
  G4double GetFluxIntegral() const { return fFluxIntegral; }

  // Muons per second crossing the launch sphere: J * pi R^2, the sphere's
  // projected area being the same from every direction.
  //
  // This is what makes a cosmic run have a live time. becquerel is 1/second, so
  // the number goes straight into the same clock a radioactive source drives, and
  // counts / live time is then the rate -- with no effective area and no angular
  // acceptance to work out, because the generated ensemble is complete.
  G4double GetRateHz() const override;

  void SetWorldRadius(G4double radius) { fWorldRadius = radius; }
  G4double GetWorldRadius() const;

  // Exposed for verification: energy in Geant4 units, angles in radians.
  void SampleFlux(G4double & energy, G4double & theta, G4double & phi) const;

protected:
  // Never reached: GenerateVertex is overridden because the position follows
  // from the sampled direction rather than from a position generator.
  void EmitParticles(G4PrimaryVertex *, const PosDir &) const override {}

private:


  void BuildCDF();

  G4ParticleDefinition * fMuon = nullptr;
  TH3D * fHistFlux = nullptr;
  // x: energy [GeV], y: theta [deg], z: phi [deg]
  G4double fFluxIntegral = 0.;   // cm^-2 s^-1, read from the histogram

  std::vector<G4double> fCDF; // normalised cumulative bin sums, size = nx*ny*nz + 1
  G4int fNBinsY = 0;
  G4int fNBinsZ = 0;

  mutable G4double fWorldRadius = 0.; // 0 => derive from the world solid
  mutable G4ThreeVector fSurfaceCentre;
  mutable G4double fSurfaceRadius = 0.;
  mutable bool fHasSurface = false;
};
