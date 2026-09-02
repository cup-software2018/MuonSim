#pragma once

#include <string>
#include <vector>

#include "AbsVertexGen.hh"
#include "G4Types.hh"
#include "SpectrumSampler.hh"

class G4ParticleDefinition;
class G4PrimaryVertex;

// Am-Be (alpha,n) calibration source: one neutron drawn from a tabulated
// spectrum, plus -- for the fraction of events that leave 12C excited -- the
// single-energy 4.44 MeV de-excitation gamma.
//
// Samples its own energies instead of composing AbsEnergyGen -- for correlated
// multi-particle sources the per-particle energies are not independent draws,
// so the kinematics belong inside the generator.
//
// Both particles are emitted isotropically and independently: for a thick
// source the neutron/gamma angular correlation is washed out.
class AmBeGen : public AbsVertexGen {
public:
  // Fraction of 9Be(alpha,n) events leaving 12C in its 4.44 MeV first excited
  // state, i.e. those accompanied by a gamma. The exact value depends on how
  // much the alpha is degraded in the Be matrix, so it varies with source
  // construction; ~0.55-0.60 is the usual range.
  static constexpr G4double kDefaultGammaProbability = 0.58;

  AmBeGen();
  ~AmBeGen() override = default;

  // Name this source is selected by in /gen/vertex. Static so the command
  // parser can match it without building a generator first.
  static const std::string & Key();
  const std::string & GetKey() const override { return Key(); }
protected:
  void EmitParticles(G4PrimaryVertex * vertex, const PosDir & where) const override;

public:

  // Defaults to the 12C first excited state, 4.438 MeV.
  void SetGammaEnergy(G4double energy) { fGammaEnergy = energy; }
  G4double GetGammaEnergy() const { return fGammaEnergy; }

  // Fraction of neutrons accompanied by the 4.44 MeV gamma. Only the 12C*
  // channel of 9Be(alpha,n)12C emits it; the ground-state channel and the
  // 3-alpha breakup do not. Set to 1.0 for a neutron+gamma pair every time.
  void SetGammaProbability(G4double probability) { fGammaProbability = probability; }
  G4double GetGammaProbability() const { return fGammaProbability; }

  // Replaces the built-in table with one read from a YAML file (same schema as
  // every other spectrum here -- see SpectrumFile.hh). Returns false and fills
  // error on any problem, leaving the built-in table in place.
  //
  // Note the file replaces the table WHOLE, including the (0, 0) anchor that
  // tapers the built-in one below its 0.25 MeV threshold: include your own
  // leading (0, 0) point if you want the same extrapolation.
  bool LoadSpectrumFile(const std::string & path, std::string & error);

  // densities are dN/dE sampled at each energy; points need not be sorted and
  // need not be normalised.
  void SetNeutronSpectrum(const std::vector<G4double> & energies,
                          const std::vector<G4double> & densities);

  // Mean of the currently loaded spectrum -- handy as a sanity check.
  G4double GetNeutronSpectrumMean() const;

  G4double SampleNeutronEnergy() const { return fNeutronEnergy.Sample(); }

private:
  G4ParticleDefinition * fNeutron = nullptr;
  G4ParticleDefinition * fGamma = nullptr;

  G4double fGammaEnergy;
  G4double fGammaProbability = kDefaultGammaProbability;

  SpectrumSampler fNeutronEnergy;
};
