#include <algorithm>
#include <cmath>

#include "CfGen.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

namespace
{
// ---- neutron multiplicity ----
// Cumulative probability of emitting at most i neutrons, i = 0..8. The implied
// differential distribution has mean 3.757, the accepted nu-bar for 252Cf SF.
const G4double kNeutronMultiplicityCDF[] = {0.002, 0.028, 0.155, 0.428, 0.732,
                                            0.917, 0.983, 0.998, 1.000};
constexpr std::size_t kMaxNeutron = sizeof(kNeutronMultiplicityCDF) /
                                        sizeof(kNeutronMultiplicityCDF[0]) - 1;

// ---- spectrum shapes, tabulated on this many bins over the ranges below ----
//
// RAT-PAC tabulates on 200 bins; over a 0-50 MeV range that is 0.25 MeV wide,
// coarse enough near the sub-MeV peak to pull the sampled neutron mean up to
// 2.157 MeV against the formula's own 2.1215. Building the table is a one-time
// cost, so use enough bins for the discretisation bias to vanish (2000 bins ->
// 2.1226 MeV, within 0.05%).
constexpr std::size_t kBins = 2000;
constexpr G4double kNeutronEMaxMeV = 50.;
constexpr G4double kGammaMultMax = 25.;
constexpr G4double kGammaEMaxMeV = 25.;

// Watt-type 252Cf fission-neutron spectrum; mean 2.12 MeV.
G4double NeutronSpectrum(G4double eMeV)
{
  if (eMeV <= 0.) return 0.;
  const G4double scale = 1. / (2. * M_PI * 0.359);
  const G4double s = std::sqrt(eMeV);
  const G4double s0 = std::sqrt(0.359);
  const G4double fMinus = std::exp(-(s - s0) * (s - s0) / 1.175);
  const G4double fPlus = std::exp(-(s + s0) * (s + s0) / 1.175);
  return scale * (fMinus - fPlus);
}

// Prompt-gamma multiplicity distribution (a fit, so continuous in the count).
G4double GammaMultiplicity(G4double n)
{
  if (n <= 9.) {
    const G4double z = (n - 7.0322) / 2.6301;
    return 0.13345 * std::exp(-0.5 * z * z);
  }
  const G4double d = std::sqrt(n) - std::sqrt(7.5987);
  return 0.11255 * std::exp(-d * d / 0.56213);
}

// Prompt-gamma energy spectrum.
G4double GammaSpectrum(G4double eMeV)
{
  if (eMeV <= 0.) return 0.;
  if (eMeV <= 0.744) {
    const G4double z = (eMeV - 0.45934) / 0.31290;
    return 1.8367 * std::exp(-0.5 * z * z);
  }
  return std::exp(0.84774 - 0.89396 * eMeV);
}
} // namespace

CfGen::CfGen()
{
  auto * table = G4ParticleTable::GetParticleTable();
  fNeutron = table->FindParticle("neutron");
  fGamma = table->FindParticle("gamma");

  fNeutronMultiplicityCDF.assign(kNeutronMultiplicityCDF,
                                 kNeutronMultiplicityCDF + kMaxNeutron + 1);

  fNeutronEnergy.SetFromFunction([](G4double e) { return NeutronSpectrum(e / MeV); }, 0.,
                                 kNeutronEMaxMeV * MeV, kBins);
  fGammaMultiplicity.SetFromFunction(GammaMultiplicity, 0., kGammaMultMax, kBins);
  fGammaEnergy.SetFromFunction([](G4double e) { return GammaSpectrum(e / MeV); }, 0.,
                               kGammaEMaxMeV * MeV, kBins);
}

const std::string & CfGen::Key()
{
  static const std::string key = "252CfSF";
  return key;
}

void CfGen::SetNeutronMultiplicity(const std::vector<G4double> & weights)
{
  fNeutronMultiplicityCDF.assign(weights.size(), 0.);
  G4double running = 0.;
  for (std::size_t i = 0; i < weights.size(); i++) {
    running += weights[i];
    fNeutronMultiplicityCDF[i] = running;
  }
  if (running > 0.) {
    for (auto & v : fNeutronMultiplicityCDF)
      v /= running;
  }
}

G4int CfGen::SampleNeutronMultiplicity() const
{
  if (fNeutronMultiplicityCDF.empty()) return 0;

  // Retry on zero: a fission that emits no neutron would leave the event with
  // gammas only, which is not what a neutron-source run is for. Follows RAT-PAC
  // and lifts the effective mean from 3.757 to 3.765.
  for (int attempt = 0; attempt < 100; attempt++) {
    const G4double r = G4UniformRand();
    std::size_t n = 0;
    while (n + 1 < fNeutronMultiplicityCDF.size() && r > fNeutronMultiplicityCDF[n])
      n++;
    if (n > 0) return (G4int)n;
  }
  return 1;
}

void CfGen::EmitParticles(G4PrimaryVertex * vertex, const PosDir & where) const
{
  if (fNeutron != nullptr) {
    const G4int nNeutron = SampleNeutronMultiplicity();
    for (G4int i = 0; i < nNeutron; i++) {
      auto * neutron = new G4PrimaryParticle(fNeutron);
      neutron->SetMomentumDirection(where.hasDirection ? where.direction : IsotropicDirection());
      neutron->SetKineticEnergy(SampleNeutronEnergy());
      vertex->SetPrimary(neutron);
    }
  }

  if (fEmitGammas && fGamma != nullptr) {
    const G4int nGamma = SampleGammaMultiplicity();
    for (G4int i = 0; i < nGamma; i++) {
      auto * gamma = new G4PrimaryParticle(fGamma);
      gamma->SetMomentumDirection(where.hasDirection ? where.direction : IsotropicDirection());
      gamma->SetKineticEnergy(SampleGammaEnergy());
      vertex->SetPrimary(gamma);
    }
  }
}
