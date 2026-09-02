#include <algorithm>
#include <cmath>
#include <utility>

#include "AmBeGen.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "Randomize.hh"
#include "SpectrumFile.hh"

namespace
{
// Measured Am-Be neutron spectrum, sampled on a uniform 0.05 MeV grid from
// 0.25 to 10.90 MeV, as a relative intensity (dN/dE).
//
// Treated as a piecewise-linear density, not as discrete lines. The energy grid
// is exactly uniform, so it is generated from the two constants below instead of
// being tabulated: that keeps the data to the intensities alone and makes an
// out-of-order energy point structurally impossible.
//
// The table starts at 0.25 MeV with a non-zero intensity (8.306) -- the
// measurement simply has no data below its threshold. To avoid a hard cutoff
// there we anchor an extra point at (0, 0) and let the existing piecewise-linear
// interpolation taper the density to zero as E -> 0.
//
// Why taper rather than extend the local trend: the tabulated intensity *rises*
// toward lower energy (8.306, 8.137, 7.641, ...), so back-extrapolating its
// slope would invent a low-energy tail growing to ~9.2 at E = 0, and holding it
// flat would leave a finite density at zero energy. The taper keeps the density
// continuous, makes it vanish at E = 0 as it must, and adds the least
// probability mass of the three (about 1.9%).
constexpr G4double kNeutronEMinMeV = 0.25;
constexpr G4double kNeutronEStepMeV = 0.05;

const G4double kNeutronIntensity[] = {
     8.306,  8.137,  7.641,  6.808,  6.010,  5.653,  5.455,  5.261,  5.083,  4.954,
     4.810,  4.612,  4.473,  4.374,  4.284,  4.165,  4.086,  3.932,  3.813,  3.689,
     3.570,  3.451,  3.352,  3.193,  3.035,  2.901,  2.841,  2.921,  3.079,  3.317,
     3.570,  3.759,  4.021,  4.240,  4.359,  4.463,  4.582,  4.631,  4.612,  4.557,
     4.488,  4.458,  4.498,  4.577,  4.676,  4.860,  5.261,  5.712,  6.367,  6.848,
     7.438,  7.904,  8.390,  9.045,  9.664, 10.339, 10.736, 11.093, 11.231, 10.988,
    10.736, 10.374, 10.021,  9.645,  9.387,  9.174,  8.975,  8.836,  8.717,  8.658,
     8.554,  8.499,  8.445,  8.385,  8.345,  8.281,  8.256,  8.207,  8.207,  8.217,
     8.256,  8.281,  8.336,  8.415,  8.519,  8.653,  8.777,  8.926,  9.104,  9.263,
     9.372,  9.516,  9.655,  9.615,  9.516,  9.322,  9.174,  9.000,  8.861,  8.658,
     8.380,  8.102,  7.894,  7.795,  7.671,  7.537,  7.379,  7.279,  7.180,  6.987,
     6.570,  6.233,  5.787,  5.331,  5.068,  4.934,  4.864,  4.785,  4.825,  4.869,
     4.934,  4.964,  4.983,  4.944,  4.934,  4.884,  4.706,  4.473,  4.255,  4.086,
     3.942,  3.848,  3.744,  3.744,  3.744,  3.818,  3.898,  3.957,  4.056,  4.136,
     4.264,  4.478,  4.686,  4.825,  4.939,  5.033,  5.073,  5.137,  5.137,  5.098,
     5.083,  5.003,  4.929,  4.830,  4.750,  4.636,  4.418,  4.121,  3.942,  3.679,
     3.322,  3.020,  2.831,  2.633,  2.385,  2.107,  1.860,  1.706,  1.507,  1.334,
     1.165,  1.076,  0.957,  0.878,  0.868,  0.917,  0.962,  1.051,  1.150,  1.245,
     1.364,  1.507,  1.651,  1.770,  1.805,  1.860,  1.939,  1.969,  2.008,  2.008,
     2.008,  1.959,  1.959,  1.919,  1.874,  1.785,  1.706,  1.617,  1.498,  1.369,
     1.240,  1.215,  1.145,  1.007,  0.873,  0.793,  0.699,  0.615,  0.516,  0.461,
     0.347,  0.238,  0.134,  0.035};

constexpr std::size_t kNeutronPoints =
    sizeof(kNeutronIntensity) / sizeof(kNeutronIntensity[0]);
static_assert(kNeutronPoints == 214, "Am-Be neutron spectrum: expected 214 grid points");
} // namespace

AmBeGen::AmBeGen()
  : fGammaEnergy(4.438 * MeV) // 12C first excited state
{
  auto * table = G4ParticleTable::GetParticleTable();
  fNeutron = table->FindParticle("neutron");
  fGamma = table->FindParticle("gamma");

  // Leading (0, 0) anchor extrapolates below the measured threshold -- see the
  // note on the table above.
  std::vector<G4double> energies, densities;
  energies.reserve(kNeutronPoints + 1);
  densities.reserve(kNeutronPoints + 1);
  energies.push_back(0.);
  densities.push_back(0.);
  for (std::size_t i = 0; i < kNeutronPoints; i++) {
    energies.push_back((kNeutronEMinMeV + (G4double)i * kNeutronEStepMeV) * MeV);
    densities.push_back(kNeutronIntensity[i]);
  }
  SetNeutronSpectrum(energies, densities);
}

const std::string & AmBeGen::Key()
{
  static const std::string key = "AmBe";
  return key;
}

void AmBeGen::SetNeutronSpectrum(const std::vector<G4double> & energies,
                                 const std::vector<G4double> & densities)
{
  fNeutronEnergy.Set(energies, densities);
}

bool AmBeGen::LoadSpectrumFile(const std::string & path, std::string & error)
{
  std::vector<G4double> energies, densities;
  G4double eMin = 0., eMax = 0.; // the table's own range is the sampling range
  if (!LoadSpectrumYaml(path, energies, densities, eMin, eMax, error)) return false;
  fNeutronEnergy.Set(energies, densities);
  return true;
}

G4double AmBeGen::GetNeutronSpectrumMean() const { return fNeutronEnergy.Mean(); }

void AmBeGen::EmitParticles(G4PrimaryVertex * vertex, const PosDir & where) const
{
  if (fNeutron != nullptr) {
    auto * neutron = new G4PrimaryParticle(fNeutron);
    neutron->SetMomentumDirection(where.hasDirection ? where.direction : IsotropicDirection());
    neutron->SetKineticEnergy(SampleNeutronEnergy());
    vertex->SetPrimary(neutron);
  }

  if (fGamma != nullptr && (fGammaProbability >= 1. || G4UniformRand() < fGammaProbability)) {
    auto * gamma = new G4PrimaryParticle(fGamma);
    gamma->SetMomentumDirection(where.hasDirection ? where.direction : IsotropicDirection());
    gamma->SetKineticEnergy(fGammaEnergy);
    vertex->SetPrimary(gamma);
  }
}
