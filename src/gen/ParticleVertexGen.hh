#pragma once

#include <memory>
#include <string>

#include "AbsEnergyGen.hh"
#include "AbsVertexGen.hh"
#include "G4ThreeVector.hh"

class G4ParticleDefinition;
class G4PrimaryVertex;

// One particle of a given species: kinetic energy from a pluggable AbsEnergyGen,
// direction from the position generator when it dictates one, otherwise from an
// explicitly set direction, otherwise isotropic.
class ParticleVertexGen : public AbsVertexGen {
public:
  ParticleVertexGen(G4ParticleDefinition * particle, std::unique_ptr<AbsEnergyGen> energyGen);
  ~ParticleVertexGen() override;

  static const std::string & Key();
  const std::string & GetKey() const override { return Key(); }

  void SetDirection(const G4ThreeVector & direction);
  bool HasDirection() const { return fHasDirection; }
  void SetPolarization(const G4ThreeVector & polarization) { fPolarization = polarization; }

protected:
  void EmitParticles(G4PrimaryVertex * vertex, const PosDir & where) const override;

private:
  G4ParticleDefinition * fParticle = nullptr;
  std::unique_ptr<AbsEnergyGen> fEnergyGen;
  bool fHasDirection = false;
  G4ThreeVector fDirection;
  G4ThreeVector fPolarization;
};
