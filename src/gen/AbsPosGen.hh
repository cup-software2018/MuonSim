#pragma once

#include <cmath>
#include <string>

#include "G4PhysicalConstants.hh"
#include "G4ThreeVector.hh"
#include "Randomize.hh"
#include "G4Types.hh"

// What a position generator hands to the vertex generator.
//
// Some position generators also fix the emission direction, because the two are
// correlated and only the position generator knows the surface it sampled:
// emitting into or out of a volume is defined relative to that volume's normal
// at the sampled point. Generators with no such constraint leave hasDirection
// false and let the source decide.
struct PosDir {
  G4ThreeVector position;
  G4ThreeVector direction;    // meaningful only when hasDirection is true
  G4ThreeVector normal;       // outward surface normal, when there is a surface
  bool hasDirection = false;
  bool hasNormal = false;
};

// How a surface source spreads its emission over the allowed hemisphere.
enum class AngularLaw {
  Isotropic, // uniform in solid angle -- decays of activity sitting on a surface
  Cosine     // ~cos(theta) about the normal -- a flux crossing the surface
};

// A direction in the hemisphere about n, following the given law.
//
// Cosine uses cos(theta) = sqrt(u), which is Lambert's law: the flux through a
// surface element goes as cos(theta), so an isotropic external field does NOT
// arrive isotropically at a surface. Drawing cos(theta) uniformly instead would
// over-weight grazing incidence. It is also exactly what a semi-infinite
// uniformly-emitting medium produces once its self-absorption is folded in.
inline G4ThreeVector HemisphereDirection(const G4ThreeVector & n, AngularLaw law)
{
  const G4double cosTheta =
      (law == AngularLaw::Cosine) ? std::sqrt(G4UniformRand()) : G4UniformRand();
  const G4double sinTheta = std::sqrt(1. - cosTheta * cosTheta);
  const G4double psi = CLHEP::twopi * G4UniformRand();

  const G4ThreeVector unit = n.unit();
  const G4ThreeVector u = unit.orthogonal().unit();
  const G4ThreeVector v = unit.cross(u);
  return sinTheta * (std::cos(psi) * u + std::sin(psi) * v) + cosTheta * unit;
}

class AbsPosGen {
public:
  AbsPosGen() = default;
  virtual ~AbsPosGen() = default;

  virtual const std::string & GetKey() const = 0;
  virtual PosDir Generate() = 0;
};
