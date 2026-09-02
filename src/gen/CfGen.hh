#pragma once

#include <string>
#include <vector>

#include "AbsVertexGen.hh"
#include "G4Types.hh"
#include "SpectrumSampler.hh"

class G4ParticleDefinition;
class G4PrimaryVertex;

// 252Cf spontaneous-fission source: every call is one fission, emitting a
// sampled multiplicity of neutrons plus the prompt fission gammas.
//
// This is deliberately separate from letting Geant4's radioactive decay handle a
// Cf252 ion (which /gen/vertex still supports as the particle name "252Cf"). That
// route spends 97% of decays on the alpha branch before a fission happens, and
// G4SFDecay never produces fission fragments; here every event is a fission.
//
// Parameterisations follow RAT-PAC's CfSource (ratpac-two, src/gen/src/CfSource.cc):
//   neutron multiplicity  cumulative table for nu = 0..8, mean 3.757
//   neutron energy        Watt-type form, mean 2.12 MeV
//   gamma multiplicity    piecewise Gaussian/exponential fit, truncated to an int
//   gamma energy          piecewise Gaussian/exponential fit
//
// Not modelled: fission fragments, and the sub-ns prompt-gamma time structure
// (RAT-PAC gives 80% of gammas a 0.01 ns and 20% a 1 ns decay constant). A single
// G4PrimaryVertex carries one time, so per-particle offsets cannot be expressed
// here; the spread is far below this detector's PMT timing anyway.
class CfGen : public AbsVertexGen {
public:
  CfGen();
  ~CfGen() override = default;

  // Name this source is selected by in /gen/vertex. Deliberately not "252Cf",
  // which stays bound to the Cf252 ion and its radioactive decay chain.
  static const std::string & Key();
  const std::string & GetKey() const override { return Key(); }

protected:
  void EmitParticles(G4PrimaryVertex * vertex, const PosDir & where) const override;

public:

  // Emit the prompt fission gammas alongside the neutrons (default true).
  void SetEmitGammas(G4double emit) { fEmitGammas = emit; }

  // Replaces the neutron multiplicity distribution. weights[i] is the relative
  // probability of i neutrons; need not be normalised.
  void SetNeutronMultiplicity(const std::vector<G4double> & weights);

  // Sampling entry points, exposed for verification.
  G4int SampleNeutronMultiplicity() const;
  G4int SampleGammaMultiplicity() const { return (G4int)fGammaMultiplicity.Sample(); }
  G4double SampleNeutronEnergy() const { return fNeutronEnergy.Sample(); }
  G4double SampleGammaEnergy() const { return fGammaEnergy.Sample(); }

  G4double GetNeutronEnergyMean() const { return fNeutronEnergy.Mean(); }
  G4double GetGammaEnergyMean() const { return fGammaEnergy.Mean(); }

private:
  G4ParticleDefinition * fNeutron = nullptr;
  G4ParticleDefinition * fGamma = nullptr;

  bool fEmitGammas = true;

  std::vector<G4double> fNeutronMultiplicityCDF; // normalised, index = neutron count
  SpectrumSampler fNeutronEnergy;
  SpectrumSampler fGammaMultiplicity;
  SpectrumSampler fGammaEnergy;
};
