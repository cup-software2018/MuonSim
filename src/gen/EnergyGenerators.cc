#include <algorithm>
#include <cmath>

#include "EnergyGenerators.hh"
#include "Randomize.hh"

// ============================================================================
// FixedEnergyGen
// ============================================================================

FixedEnergyGen::FixedEnergyGen(G4double energy)
  : fEnergy(energy)
{
}

const std::string & FixedEnergyGen::GetKey() const
{
  static const std::string key = "Fixed";
  return key;
}

// ============================================================================
// UniformEnergyGen
// ============================================================================

UniformEnergyGen::UniformEnergyGen(G4double min, G4double max)
  : fMin(min),
    fMax(max)
{
}

const std::string & UniformEnergyGen::GetKey() const
{
  static const std::string key = "Uniform";
  return key;
}

G4double UniformEnergyGen::GenerateEnergy() { return fMin + G4UniformRand() * (fMax - fMin); }

// ============================================================================
// GaussEnergyGen
// ============================================================================

GaussEnergyGen::GaussEnergyGen(G4double mean, G4double sigma)
  : fMean(mean),
    fSigma(sigma)
{
}

const std::string & GaussEnergyGen::GetKey() const
{
  static const std::string key = "Gauss";
  return key;
}

G4double GaussEnergyGen::GenerateEnergy()
{
  // Rejection-sample the tail below zero rather than folding it onto zero,
  // which would put a spurious delta at E = 0.
  for (int attempt = 0; attempt < 1000; attempt++) {
    const G4double energy = G4RandGauss::shoot(fMean, fSigma);
    if (energy >= 0.) return energy;
  }
  return 0.;
}

// ============================================================================
// ExpEnergyGen
// ============================================================================

ExpEnergyGen::ExpEnergyGen(G4double e0)
  : fE0(e0)
{
}

const std::string & ExpEnergyGen::GetKey() const
{
  static const std::string key = "Exp";
  return key;
}

G4double ExpEnergyGen::GenerateEnergy()
{
  G4double r = G4UniformRand();
  if (r <= 0.) r = 1.e-16; // guard log(0)
  return -fE0 * std::log(r);
}

// ============================================================================
// PowerLawEnergyGen
// ============================================================================

PowerLawEnergyGen::PowerLawEnergyGen(G4double index, G4double min, G4double max)
  : fIndex(index),
    fMin(min),
    fMax(max)
{
}

const std::string & PowerLawEnergyGen::GetKey() const
{
  static const std::string key = "PowerLaw";
  return key;
}

G4double PowerLawEnergyGen::GenerateEnergy()
{
  const G4double r = G4UniformRand();

  // index == -1 integrates to a log, so it needs its own inverse transform.
  if (std::abs(fIndex + 1.) < 1.e-9) {
    if (fMin <= 0.) return fMin;
    return fMin * std::pow(fMax / fMin, r);
  }

  const G4double p = fIndex + 1.;
  const G4double lo = std::pow(fMin, p);
  const G4double hi = std::pow(fMax, p);
  return std::pow(lo + r * (hi - lo), 1. / p);
}

// ============================================================================
// SpectrumEnergyGen
// ============================================================================

SpectrumEnergyGen::SpectrumEnergyGen(const std::vector<G4double> & energies,
                                     const std::vector<G4double> & weights)
  : fEnergies(energies)
{
  fCDF.assign(weights.size() + 1, 0.);
  for (std::size_t i = 0; i < weights.size(); i++)
    fCDF[i + 1] = fCDF[i] + weights[i];

  const G4double total = fCDF.back();
  if (total > 0.) {
    for (auto & v : fCDF)
      v /= total;
  }
}

const std::string & SpectrumEnergyGen::GetKey() const
{
  static const std::string key = "Spectrum";
  return key;
}

G4double SpectrumEnergyGen::GenerateEnergy()
{
  if (fEnergies.empty()) return 0.;

  const G4double r = G4UniformRand();
  std::size_t idx = (std::size_t)(std::upper_bound(fCDF.begin(), fCDF.end(), r) - fCDF.begin()) - 1;
  if (idx >= fEnergies.size()) idx = fEnergies.size() - 1;
  return fEnergies[idx];
}
