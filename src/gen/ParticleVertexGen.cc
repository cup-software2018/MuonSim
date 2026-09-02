#include "G4ParticleDefinition.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "ParticleVertexGen.hh"

ParticleVertexGen::ParticleVertexGen(G4ParticleDefinition * particle,
                                     std::unique_ptr<AbsEnergyGen> energyGen)
  : fParticle(particle),
    fEnergyGen(std::move(energyGen))
{
}

ParticleVertexGen::~ParticleVertexGen() = default;

const std::string & ParticleVertexGen::Key()
{
  static const std::string key = "Particle";
  return key;
}

void ParticleVertexGen::SetDirection(const G4ThreeVector & direction)
{
  if (direction.mag2() <= 0.) return;
  fDirection = direction.unit();
  fHasDirection = true;
}

void ParticleVertexGen::EmitParticles(G4PrimaryVertex * vertex, const PosDir & where) const
{
  if (fParticle == nullptr) return;

  // The surface constraint wins: it is a property of where the vertex is.
  G4ThreeVector direction = where.hasDirection ? where.direction
                            : fHasDirection    ? fDirection
                                               : IsotropicDirection();

  auto * primary = new G4PrimaryParticle(fParticle);
  primary->SetMomentumDirection(direction);
  primary->SetKineticEnergy(fEnergyGen ? fEnergyGen->GenerateEnergy() : 0.);
  primary->SetPolarization(fPolarization);
  vertex->SetPrimary(primary);
}
