#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "RockGammaGen.hh"
#include "SpectrumFile.hh"

RockGammaGen::RockGammaGen()
{
  fGamma = G4ParticleTable::GetParticleTable()->FindParticle("gamma");
}

const std::string & RockGammaGen::Key()
{
  static const std::string key = "rockgamma";
  return key;
}

void RockGammaGen::SetSpectrum(const std::vector<G4double> & energies,
                               const std::vector<G4double> & flux)
{
  fSpectrum.Set(energies, flux);
}

bool RockGammaGen::LoadSpectrumFile(const std::string & path, std::string & error)
{
  std::vector<G4double> energies, flux;
  G4double eMin = 0., eMax = 0.; // a surface flux has no threshold to clamp to
  if (!LoadSpectrumYaml(path, energies, flux, eMin, eMax, error)) return false;
  fSpectrum.Set(energies, flux);
  return true;
}

void RockGammaGen::EmitParticles(G4PrimaryVertex * vertex, const PosDir & where) const
{
  if (fGamma == nullptr || fSpectrum.Empty()) return;

  auto * primary = new G4PrimaryParticle(fGamma);
  primary->SetMomentumDirection(where.hasDirection ? where.direction : IsotropicDirection());
  primary->SetKineticEnergy(SampleEnergy());
  vertex->SetPrimary(primary);
}
