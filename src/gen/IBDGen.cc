#include <algorithm>
#include <cmath>

#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4PhysicalConstants.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include "IBDGen.hh"
#include "Randomize.hh"
#include "SpectrumFile.hh"

namespace
{
const G4double kDelta = neutron_mass_c2 - proton_mass_c2;
const G4double kGFermi = 1.16639e-11 / MeV / MeV;

// Flat-flux fallback range, used until a real spectrum is supplied.
constexpr G4double kDefaultEMax = 10. * MeV;

// 10% headroom on the rejection envelope, as in the reference.
constexpr G4double kEnvelopeMargin = 1.1;
} // namespace

IBDGen::IBDGen()
{
  auto * table = G4ParticleTable::GetParticleTable();
  fPositron = table->FindParticle("e+");
  fNeutron = table->FindParticle("neutron");

  // Flat flux over [threshold, 10 MeV]: the cross section alone then shapes the
  // spectrum. Replace with SetNeutrinoSpectrum() for a real source.
  const G4double lo = ThresholdEnergy();
  fFlux.Set({lo, kDefaultEMax}, {1., 1.});
  UpdateSamplingBounds();
}

const std::string & IBDGen::Key()
{
  static const std::string key = "IBD";
  return key;
}

G4double IBDGen::ThresholdEnergy()
{
  return ((proton_mass_c2 + kDelta + electron_mass_c2) *
              (proton_mass_c2 + electron_mass_c2 + kDelta) -
          proton_mass_c2 * proton_mass_c2) /
         2. / proton_mass_c2;
}

G4double IBDGen::PositronEnergy(G4double eNu, G4double cosThetaLab)
{
  // Zeroth order: infinite nucleon mass.
  const G4double e0 = eNu - kDelta;
  if (e0 <= electron_mass_c2) return electron_mass_c2;
  const G4double p0 = std::sqrt(e0 * e0 - electron_mass_c2 * electron_mass_c2);
  const G4double v0 = p0 / e0;

  // First order correction for finite nucleon mass.
  const G4double ySquared = (kDelta * kDelta - electron_mass_c2 * electron_mass_c2) / 2.;
  G4double e1 = e0 * (1. - eNu / proton_mass_c2 * (1. - v0 * cosThetaLab)) -
                ySquared / proton_mass_c2;
  if (e1 < electron_mass_c2) e1 = electron_mass_c2;
  return e1;
}

G4double IBDGen::CrossSection(G4double eNu, G4double cosThetaLab)
{
  if (eNu < ThresholdEnergy()) return 0.;

  const G4double cosThetaC = (0.9741 + 0.9756) / 2.;
  const G4double radCor = 0.024;
  const G4double sigma0 = kGFermi * kGFermi * cosThetaC * cosThetaC / pi * (1. + radCor);

  const G4double f = 1.00;
  const G4double f2 = 3.706;
  const G4double g = 1.26;

  G4double e0 = eNu - kDelta;
  if (e0 < electron_mass_c2) e0 = electron_mass_c2;
  const G4double p0 = std::sqrt(e0 * e0 - electron_mass_c2 * electron_mass_c2);
  const G4double v0 = p0 / e0;

  const G4double e1 = PositronEnergy(eNu, cosThetaLab);
  const G4double p1 = std::sqrt(e1 * e1 - electron_mass_c2 * electron_mass_c2);
  const G4double v1 = p1 / e1;

  if (v0 <= 0.) return 0.;

  const G4double me2 = electron_mass_c2 * electron_mass_c2;
  const G4double gamma =
      2. * (f + f2) * g * ((2. * e0 + kDelta) * (1. - v0 * cosThetaLab) - me2 / e0) +
      (f * f + g * g) * (kDelta * (1. + v0 * cosThetaLab) + me2 / e0) +
      (f * f + 3. * g * g) * ((e0 + kDelta) * (1. - cosThetaLab / v0) - kDelta) +
      (f * f - g * g) * ((e0 + kDelta) * (1. - cosThetaLab / v0) - kDelta) * v0 * cosThetaLab;

  G4double xc = ((f * f + 3. * g * g) + (f * f - g * g) * v1 * cosThetaLab) * e1 * p1 -
                gamma / proton_mass_c2 * e0 * p0;
  xc *= sigma0 / 2. * hbarc * hbarc;
  return xc;
}

void IBDGen::SetNeutrinoSpectrum(const std::vector<G4double> & energies,
                                 const std::vector<G4double> & flux)
{
  fFlux.Set(energies, flux);
  UpdateSamplingBounds();
}

void IBDGen::SetEnergyRange(G4double eMin, G4double eMax)
{
  fEMinRequest = eMin;
  fEMaxRequest = eMax;
  UpdateSamplingBounds();
}

bool IBDGen::LoadSpectrumFile(const std::string & path, std::string & error)
{
  std::vector<G4double> energies, flux;
  G4double eMin = 0., eMax = 0.;
  if (!LoadSpectrumYaml(path, energies, flux, eMin, eMax, error)) return false;

  fFlux.Set(energies, flux);
  SetEnergyRange(eMin, eMax);

  if (fEMax <= fEMin) {
    error = path + ": sampling window is empty after clamping to the IBD threshold (" +
            std::to_string(ThresholdEnergy() / MeV) + " MeV)";
    return false;
  }
  return true;
}

void IBDGen::UpdateSamplingBounds()
{
  // An explicit request wins, but never below threshold or outside the table.
  const G4double tableMin = fFlux.MinX();
  const G4double tableMax = fFlux.MaxX();
  fEMin = std::max(ThresholdEnergy(), fEMinRequest > 0. ? fEMinRequest : tableMin);
  fEMax = (fEMaxRequest > 0.) ? std::min(fEMaxRequest, tableMax) : tableMax;
  fFluxMax = kEnvelopeMargin * fFlux.MaxDensity();

  // The cross section grows with energy and peaks at cos theta = -1, so this is
  // the envelope over the whole sampling region.
  fXCMax = CrossSection(fEMax, -1.);
}

void IBDGen::SampleInteraction(G4double & eNu, G4double & cosThetaLab) const
{
  if (fEMax <= fEMin || fXCMax <= 0. || fFluxMax <= 0.) {
    eNu = fEMin;
    cosThetaLab = 0.;
    return;
  }

  // Rejection sampling on (E_nu, cos theta) against flux x dsigma/dcos.
  for (int attempt = 0; attempt < 1000000; attempt++) {
    eNu = fEMin + (fEMax - fEMin) * G4UniformRand();
    cosThetaLab = -1. + 2. * G4UniformRand();
    const G4double envelope = fXCMax * fFluxMax * G4UniformRand();
    if (CrossSection(eNu, cosThetaLab) * fFlux.Density(eNu) > envelope) return;
  }
  cosThetaLab = 0.;
}

void IBDGen::EmitParticles(G4PrimaryVertex * vertex, const PosDir & where) const
{
  G4double eNu = 0., cosThetaLab = 0.;
  SampleInteraction(eNu, cosThetaLab);

  // A dictated direction orients the incoming antineutrino, so the whole
  // correlated e+/n pair is oriented with it and its internal angles survive.
  const G4ThreeVector nuDir = where.hasDirection      ? where.direction
                              : fHasNeutrinoDirection ? fNeutrinoDirection
                                                      : IsotropicDirection();

  const G4double e1 = PositronEnergy(eNu, cosThetaLab);
  const G4double p1 = std::sqrt(e1 * e1 - electron_mass_c2 * electron_mass_c2);

  // Positron direction: rotate off the neutrino direction by theta, about a
  // randomly oriented axis perpendicular to it.
  G4ThreeVector posMomentum = p1 * nuDir;
  G4ThreeVector rotationAxis = nuDir.orthogonal();
  rotationAxis.rotate(twopi * G4UniformRand(), nuDir);
  posMomentum.rotate(std::acos(cosThetaLab), rotationAxis);

  // Neutron takes the balance of momentum.
  const G4ThreeVector neutronMomentum = eNu * nuDir - posMomentum;
  const G4double neutronE =
      std::sqrt(neutronMomentum.mag2() + neutron_mass_c2 * neutron_mass_c2);

  if (fPositron != nullptr) {
    auto * positron = new G4PrimaryParticle(fPositron);
    positron->SetMomentumDirection(posMomentum.unit());
    positron->SetKineticEnergy(e1 - electron_mass_c2);
    vertex->SetPrimary(positron);
  }

  if (fNeutron != nullptr) {
    auto * neutron = new G4PrimaryParticle(fNeutron);
    neutron->SetMomentumDirection(neutronMomentum.unit());
    neutron->SetKineticEnergy(neutronE - neutron_mass_c2);
    vertex->SetPrimary(neutron);
  }
}
