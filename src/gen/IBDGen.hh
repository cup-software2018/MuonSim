#pragma once

#include <string>
#include <vector>

#include "AbsVertexGen.hh"
#include "G4ThreeVector.hh"
#include "G4Types.hh"
#include "SpectrumSampler.hh"

class G4ParticleDefinition;
class G4PrimaryVertex;

// Inverse beta decay, nu_e_bar + p -> e+ + n. Emits the correlated positron and
// neutron from one vertex.
//
// Physics follows RAT-PAC's IBDgen (ratpac-two, src/gen/src/IBDgen.cc), i.e. the
// Strumia & Vissani differential cross section with first-order nucleon-mass
// corrections, and positron/neutron kinematics from momentum conservation. The
// (E_nu, cos theta) pair is drawn by rejection sampling against flux x dsigma.
//
// The antineutrino spectrum is an INPUT, not a property of the interaction: it
// depends entirely on the source (reactor, geoneutrino, ...). RAT-PAC reads it
// from a database; here it comes from a YAML file named after the key in the
// command:
//
//   /gen/vertex <position...> IBD <spectrum.yml>
//
// with the file supplying the sampling window and the tabulated flux:
//
//   energy_unit: MeV      # optional, default MeV
//   energy_min: 1.806     # clamped up to the IBD threshold
//   energy_max: 10.0
//   energy: [ ... ]       # tabulated flux, interpolated piecewise linearly;
//   flux:   [ ... ]       # need not be normalised
//
// /gen/vertex REQUIRES that file: unlike Am-Be there is no single standard
// antineutrino spectrum (reactor, geoneutrino, ... all differ), so a built-in
// default would quietly produce a non-physical source.
//
// A default-constructed IBDGen does start with a flat flux over
// [threshold, 10 MeV], which leaves the cross section alone to shape the output.
// That is reachable only from C++, and exists so the kinematics can be exercised
// in isolation -- it is NOT a physical source spectrum.
class IBDGen : public AbsVertexGen {
public:
  IBDGen();
  ~IBDGen() override = default;

  static const std::string & Key();
  const std::string & GetKey() const override { return Key(); }

protected:
  void EmitParticles(G4PrimaryVertex * vertex, const PosDir & where) const override;

public:

  // Reads energy_min / energy_max / energy / flux from a YAML file. Returns
  // false and fills error on any problem, leaving the previous spectrum intact.
  bool LoadSpectrumFile(const std::string & path, std::string & error);

  // flux[i] is the relative antineutrino intensity at energies[i]; it need not
  // be normalised. Values below threshold are ignored.
  void SetNeutrinoSpectrum(const std::vector<G4double> & energies,
                           const std::vector<G4double> & flux);

  // Sampling window. Clamped to the IBD threshold; pass 0 to fall back to the
  // tabulated spectrum's own range.
  void SetEnergyRange(G4double eMin, G4double eMax);

  // Fixed incoming antineutrino direction (e.g. toward a reactor). Without this
  // each event draws an isotropic direction.
  void SetNeutrinoDirection(const G4ThreeVector & direction);
  void SetIsotropicNeutrinos() { fHasNeutrinoDirection = false; }

  // IBD threshold in the lab, ~1.806 MeV.
  static G4double ThresholdEnergy();

  // Positron energy including the first-order nucleon-mass correction.
  static G4double PositronEnergy(G4double eNu, G4double cosThetaLab);

  // Differential cross section d(sigma)/d(cos theta), zero below threshold.
  static G4double CrossSection(G4double eNu, G4double cosThetaLab);

  // Draws a correlated (E_nu, cos theta) pair. Exposed for verification.
  void SampleInteraction(G4double & eNu, G4double & cosThetaLab) const;

  G4double GetEMin() const { return fEMin; }
  G4double GetEMax() const { return fEMax; }

private:
  void UpdateSamplingBounds();

  G4ParticleDefinition * fPositron = nullptr;
  G4ParticleDefinition * fNeutron = nullptr;

  SpectrumSampler fFlux; // evaluated, not sampled: supplies the flux weight
  G4double fEMinRequest = 0.; // 0 => use the spectrum's own range
  G4double fEMaxRequest = 0.;
  G4double fEMin = 0.;
  G4double fEMax = 0.;
  G4double fFluxMax = 0.;
  G4double fXCMax = 0.;

  bool fHasNeutrinoDirection = false;
  G4ThreeVector fNeutrinoDirection;
};
