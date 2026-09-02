#include <algorithm>
#include <stdexcept>
#include <yaml-cpp/yaml.h>

#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"
#include "SpectrumFile.hh"

namespace
{
// True if name is a known Geant4 energy unit, so a typo is reported rather than
// silently treated as MeV.
bool IsEnergyUnit(const std::string & name)
{
  for (auto * cat : G4UnitDefinition::GetUnitsTable()) {
    if (cat == nullptr || cat->GetName() != "Energy") continue;
    for (auto * u : cat->GetUnitsList())
      if (u != nullptr && (name == u->GetName() || name == u->GetSymbol())) return true;
  }
  return false;
}
} // namespace

bool LoadSpectrumYaml(const std::string & path, std::vector<G4double> & energies,
                      std::vector<G4double> & flux, G4double & eMin, G4double & eMax,
                      std::string & error)
{
  YAML::Node root;
  try {
    root = YAML::LoadFile(path);
  }
  catch (const std::exception & e) {
    error = "cannot read YAML spectrum file '" + path + "': " + e.what();
    return false;
  }

  G4double unit = MeV;
  if (root["energy_unit"]) {
    const std::string name = root["energy_unit"].as<std::string>();
    if (!IsEnergyUnit(name)) {
      error = "unknown energy_unit '" + name + "' in " + path;
      return false;
    }
    unit = G4UnitDefinition::GetValueOf(name);
  }

  if (!root["energy"] || !root["flux"]) {
    error = path + " must contain both 'energy' and 'flux' sequences";
    return false;
  }

  std::vector<G4double> e, f;
  try {
    for (const auto & v : root["energy"])
      e.push_back(v.as<G4double>() * unit);
    for (const auto & v : root["flux"])
      f.push_back(v.as<G4double>());
  }
  catch (const std::exception & ex) {
    error = std::string("malformed number in ") + path + ": " + ex.what();
    return false;
  }

  if (e.size() != f.size()) {
    error = path + ": 'energy' has " + std::to_string(e.size()) + " entries but 'flux' has " +
            std::to_string(f.size());
    return false;
  }
  if (e.size() < 2) {
    error = path + ": need at least two spectrum points";
    return false;
  }

  G4double maxFlux = 0.;
  for (const auto & v : f)
    maxFlux = std::max(maxFlux, v);
  if (maxFlux <= 0.) {
    error = path + ": all flux values are zero";
    return false;
  }

  G4double lo = 0., hi = 0.;
  if (root["energy_min"]) lo = root["energy_min"].as<G4double>() * unit;
  if (root["energy_max"]) hi = root["energy_max"].as<G4double>() * unit;
  if (lo > 0. && hi > 0. && hi <= lo) {
    error = path + ": energy_max is not above energy_min";
    return false;
  }

  energies = std::move(e);
  flux = std::move(f);
  eMin = lo;
  eMax = hi;
  return true;
}
