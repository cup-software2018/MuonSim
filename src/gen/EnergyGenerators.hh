#pragma once

#include <string>
#include <vector>

#include "AbsEnergyGen.hh"
#include "G4Types.hh"

class FixedEnergyGen : public AbsEnergyGen {
public:
  FixedEnergyGen() = default;
  explicit FixedEnergyGen(G4double energy);

  ~FixedEnergyGen() override = default;

  const std::string & GetKey() const override;
  G4double GenerateEnergy() override { return fEnergy; }

private:
  G4double fEnergy = 0.;
};

// Flat sampling between [min, max).
class UniformEnergyGen : public AbsEnergyGen {
public:
  UniformEnergyGen() = default;
  UniformEnergyGen(G4double min, G4double max);

  ~UniformEnergyGen() override = default;

  const std::string & GetKey() const override;
  G4double GenerateEnergy() override;

private:
  G4double fMin = 0.;
  G4double fMax = 0.;
};

// Gaussian, truncated at zero (a negative kinetic energy is unphysical).
class GaussEnergyGen : public AbsEnergyGen {
public:
  GaussEnergyGen() = default;
  GaussEnergyGen(G4double mean, G4double sigma);

  ~GaussEnergyGen() override = default;

  const std::string & GetKey() const override;
  G4double GenerateEnergy() override;

private:
  G4double fMean = 0.;
  G4double fSigma = 0.;
};

// Exponential, dN/dE ~ exp(-E/E0), mean E0.
class ExpEnergyGen : public AbsEnergyGen {
public:
  ExpEnergyGen() = default;
  explicit ExpEnergyGen(G4double e0);

  ~ExpEnergyGen() override = default;

  const std::string & GetKey() const override;
  G4double GenerateEnergy() override;

private:
  G4double fE0 = 0.;
};

// Power law, dN/dE ~ E^index on [min, max]. index == -1 is handled separately.
class PowerLawEnergyGen : public AbsEnergyGen {
public:
  PowerLawEnergyGen() = default;
  PowerLawEnergyGen(G4double index, G4double min, G4double max);

  ~PowerLawEnergyGen() override = default;

  const std::string & GetKey() const override;
  G4double GenerateEnergy() override;

private:
  G4double fIndex = 0.;
  G4double fMin = 0.;
  G4double fMax = 0.;
};

// Discrete energy/weight list (e.g. alpha or gamma lines), inverse-CDF
// sampling -- same technique as the muon flux TH3D in PrimaryGeneratorAction.
class SpectrumEnergyGen : public AbsEnergyGen {
public:
  SpectrumEnergyGen() = default;
  SpectrumEnergyGen(const std::vector<G4double> & energies, const std::vector<G4double> & weights);

  ~SpectrumEnergyGen() override = default;

  const std::string & GetKey() const override;
  G4double GenerateEnergy() override;

private:
  std::vector<G4double> fEnergies;
  std::vector<G4double> fCDF; // normalised cumulative weights, size = n+1
};
