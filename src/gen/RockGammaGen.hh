#pragma once

#include <string>
#include <vector>

#include "AbsVertexGen.hh"
#include "G4Types.hh"
#include "SpectrumSampler.hh"

class G4ParticleDefinition;
class G4PrimaryVertex;

// Gammas from the rock surrounding the cavern.
//
// The position and the inward, cosine-weighted direction come from the position
// generator (typically "onvolume in World", whose surface is the cavern
// boundary), so this class only owns the energy spectrum. The cosine weighting
// matters: an isotropic external field does not arrive isotropically at a
// surface -- the flux through a surface element goes as cos(theta) -- and it is
// also exactly what a semi-infinite uniformly-emitting rock produces once its
// own self-absorption is folded in.
class RockGammaGen : public AbsVertexGen {
public:
  RockGammaGen();
  ~RockGammaGen() override = default;

  static const std::string & Key();
  const std::string & GetKey() const override { return Key(); }

  bool LoadSpectrumFile(const std::string & path, std::string & error);
  void SetSpectrum(const std::vector<G4double> & energies, const std::vector<G4double> & flux);

  bool HasSpectrum() const { return !fSpectrum.Empty(); }
  G4double GetSpectrumMean() const { return fSpectrum.Mean(); }
  G4double SampleEnergy() const { return fSpectrum.Sample(); }

protected:
  void EmitParticles(G4PrimaryVertex * vertex, const PosDir & where) const override;

private:
  G4ParticleDefinition * fGamma = nullptr;
  SpectrumSampler fSpectrum;
};
