#include <algorithm>
#include <fstream>
#include <sstream>

#include <yaml-cpp/yaml.h>

#include "G4Exception.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4OpticalSurface.hh"
#include "G4SurfaceProperty.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"
#include "MaterialPropertyFile.hh"

namespace
{

std::vector<std::string> gFiles;
bool gApplied = false;
std::string gDigest;

// A cheap content digest -- FNV-1a over the file's bytes. Not a cryptographic
// hash: the job is to tell two versions of a table apart in a log, and 64 bits of
// FNV does that without pulling in a hashing library.
std::string FileDigest(const std::string & path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) return "";
  unsigned long long h = 1469598103934665603ULL;
  char c = 0;
  std::size_t n = 0;
  while (in.get(c)) {
    h ^= (unsigned char)c;
    h *= 1099511628211ULL;
    n++;
  }
  std::ostringstream os;
  os << std::hex << h << std::dec << "/" << n << "B";
  return os.str();
}

// Unit lookup through Geant4's own table, so "eV", "nm", "ns" and the rest mean
// exactly what they mean everywhere else. Returns 0 when the name is unknown.
G4double UnitValue(const std::string & name, const std::string & category)
{
  if (name.empty()) return 0.;
  for (auto * table : G4UnitDefinition::GetUnitsTable()) {
    if (table == nullptr || table->GetName() != category) continue;
    for (auto * u : table->GetUnitsList())
      if (u != nullptr && (name == u->GetName() || name == u->GetSymbol())) return u->GetValue();
  }
  return 0.;
}

// Birks constants are quoted two ways and they differ by two orders of magnitude,
// so the unit is required and the composite spellings are listed rather than
// guessed. G4's unit table has no Length/Energy category to defer to.
//
//   mm/MeV, cm/MeV        what SetBirksConstant takes
//   g/cm2/MeV, mg/cm2/MeV the mass-thickness form, kB * density -- divided by the
//                         material's density on the way in, which is what makes it
//                         transferable between materials of different density
bool ParseBirks(const std::string & text, const G4Material * material, G4double & out,
                std::string & error)
{
  std::istringstream is(text);
  G4double value = 0.;
  std::string unit;
  if (!(is >> value) || !(is >> unit)) {
    error = "birks_constant needs a value and a unit, e.g. '0.126 mm/MeV'";
    return false;
  }
  if (unit == "mm/MeV") { out = value * mm / MeV; }
  else if (unit == "cm/MeV") { out = value * cm / MeV; }
  else if (unit == "g/cm2/MeV" || unit == "mg/cm2/MeV") {
    const G4double scale = (unit[0] == 'm') ? milligram : gram;
    const G4double density = material->GetDensity();
    if (density <= 0.) {
      error = "cannot convert a mass-thickness Birks constant: material has no density";
      return false;
    }
    out = value * scale / (cm * cm) / MeV / density;
  }
  else {
    error = "unknown Birks unit '" + unit + "' -- use mm/MeV, cm/MeV, g/cm2/MeV or mg/cm2/MeV";
    return false;
  }
  return true;
}

// A scalar with an optional unit: "2.1 ns", or bare for a dimensionless one.
bool ParseScalar(const YAML::Node & node, G4double & out, std::string & error)
{
  const std::string text = node.as<std::string>();
  std::istringstream is(text);
  G4double value = 0.;
  if (!(is >> value)) {
    error = "'" + text + "' is not a number";
    return false;
  }
  std::string unit;
  if (is >> unit) {
    // Try every category: the caller does not know whether this key is a time, an
    // energy or dimensionless, and Geant4's key list does not say either.
    G4double u = 0.;
    for (const char * cat : {"Time", "Energy", "Length", "Volumic Mass", "Frequency"}) {
      u = UnitValue(unit, cat);
      if (u > 0.) break;
    }
    if (u <= 0.) {
      error = "unknown unit '" + unit + "'";
      return false;
    }
    value *= u;
  }
  out = value;
  return true;
}

// Every key a node is allowed to carry. Anything else is a typo, and a typo has to
// be an error rather than a shrug: a misspelled 'properties' would leave the
// material with no optical properties at all, and the run would go on quietly
// making no light.
bool CheckKeys(const YAML::Node & node, const std::vector<std::string> & allowed,
               const std::string & where, std::string & error)
{
  if (!node.IsMap()) {
    error = where + ": expected a mapping";
    return false;
  }
  for (const auto & kv : node) {
    const std::string key = kv.first.as<std::string>();
    if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
      std::string list;
      for (const auto & a : allowed) list += (list.empty() ? "" : ", ") + a;
      error = where + ": unknown key '" + key + "'. Allowed here: " + list;
      return false;
    }
  }
  return true;
}

G4OpticalSurface * FindSurface(const std::string & name)
{
  const auto * table = G4SurfaceProperty::GetSurfacePropertyTable();
  if (table == nullptr) return nullptr;
  for (auto * prop : *table)
    if (prop != nullptr && prop->GetName() == name)
      return dynamic_cast<G4OpticalSurface *>(prop);
  return nullptr;
}

// One property table entry: the x grid, the values, and the flags that decide how
// to read them.
bool ReadProperty(const std::string & key, const YAML::Node & node,
                  G4MaterialPropertiesTable * mpt, const std::string & owner,
                  std::string & error)
{
  if (!CheckKeys(node,
                 {"energy", "wavelength", "value", "energy_unit", "wavelength_unit",
                  "value_unit", "spectrum", "new_key"},
                 owner + "/" + key, error))
    return false;

  const bool declaredNew = node["new_key"] && node["new_key"].as<bool>();
  const bool isSpectrum = node["spectrum"] && node["spectrum"].as<bool>();

  // Look the name up in the catalogue rather than through GetPropertyIndex, which
  // answers an unknown key with a FatalException -- killing the run before the
  // message below, which is the whole point of checking, can be printed.
  const auto & names = mpt->GetMaterialPropertyNames();
  const bool known = std::find(names.begin(), names.end(), key) != names.end();

  if (!declaredNew && !known) {
    error = owner + ": '" + key +
            "' is not a Geant4 property name. Fix the spelling, or set new_key: true "
            "if it really is a property of your own";
    return false;
  }

  // Only ask for a new key when there is not one already. Passing createNewKey for
  // an existing name registers the name a SECOND time, which leaves two entries
  // competing and whoever looks the property up reading the wrong one. That is not
  // hypothetical: the built-in tables create KINDEX and THICKNESS this way, so a
  // file restating them would double them.
  const bool newKey = declaredNew && !known;

  const bool byWavelength = (bool)node["wavelength"];
  const YAML::Node grid = byWavelength ? node["wavelength"] : node["energy"];
  if (!grid || !node["value"]) {
    error = owner + "/" + key + ": needs 'energy' (or 'wavelength') and 'value'";
    return false;
  }
  if (grid.size() != node["value"].size()) {
    error = owner + "/" + key + ": " + std::to_string(grid.size()) + " x values against " +
            std::to_string(node["value"].size()) + " y values";
    return false;
  }
  if (grid.size() < 2) {
    error = owner + "/" + key + ": needs at least two points";
    return false;
  }

  const std::string xUnitName =
      byWavelength ? (node["wavelength_unit"] ? node["wavelength_unit"].as<std::string>() : "nm")
                   : (node["energy_unit"] ? node["energy_unit"].as<std::string>() : "eV");
  const G4double xUnit = UnitValue(xUnitName, "Length");
  const G4double eUnit = UnitValue(xUnitName, "Energy");
  if (byWavelength && xUnit <= 0.) {
    error = owner + "/" + key + ": unknown wavelength unit '" + xUnitName + "'";
    return false;
  }
  if (!byWavelength && eUnit <= 0.) {
    error = owner + "/" + key + ": unknown energy unit '" + xUnitName + "'";
    return false;
  }

  G4double yUnit = 1.;
  if (node["value_unit"]) {
    const std::string yName = node["value_unit"].as<std::string>();
    for (const char * cat : {"Length", "Time", "Energy"}) {
      yUnit = UnitValue(yName, cat);
      if (yUnit > 0.) break;
    }
    if (yUnit <= 0.) {
      error = owner + "/" + key + ": unknown value unit '" + yName + "'";
      return false;
    }
  }

  std::vector<std::pair<G4double, G4double>> points;
  points.reserve(grid.size());
  for (std::size_t i = 0; i < grid.size(); i++) {
    const G4double x = grid[i].as<double>();
    G4double y = node["value"][i].as<double>() * yUnit;
    G4double energy = 0.;
    if (byWavelength) {
      const G4double lambda = x * xUnit;
      if (lambda <= 0.) {
        error = owner + "/" + key + ": wavelength must be positive";
        return false;
      }
      energy = CLHEP::h_Planck * CLHEP::c_light / lambda;
      // A density in lambda becomes a density in E only with the Jacobian.
      if (isSpectrum) y *= lambda * lambda / (CLHEP::h_Planck * CLHEP::c_light);
    }
    else {
      energy = x * eUnit;
    }
    points.emplace_back(energy, y);
  }

  std::sort(points.begin(), points.end(),
            [](const auto & a, const auto & b) { return a.first < b.first; });
  for (std::size_t i = 1; i < points.size(); i++) {
    if (points[i].first <= points[i - 1].first) {
      error = owner + "/" + key + ": two points share the same photon energy";
      return false;
    }
  }

  std::vector<G4double> energies, values;
  energies.reserve(points.size());
  values.reserve(points.size());
  for (const auto & p : points) {
    energies.push_back(p.first);
    values.push_back(p.second);
  }

  mpt->AddProperty(key, energies, values, newKey);
  return true;
}

} // namespace

// Registering after the files have been applied would put a file in the list that
// nothing will ever read -- the silent-no-op that made /material/load a trap when
// it was issued after /run/initialize. Refuse instead: a property file the caller
// believes is in effect but which was never applied puts every optical number in
// the run in doubt.
void RegisterMaterialPropertyFile(const std::string & path)
{
  if (gApplied) {
    G4Exception("RegisterMaterialPropertyFile", "MAT001", FatalException,
                ("'" + path +
                 "' comes too late -- the optical properties were applied when the "
                 "geometry was built. Name it before the run starts, with -m.")
                    .c_str());
    return;
  }
  gFiles.push_back(path);
}

const std::vector<std::string> & RegisteredMaterialPropertyFiles() { return gFiles; }

const std::string & MaterialPropertyFileDigest() { return gDigest; }

bool ApplyMaterialPropertyFiles(std::string & error)
{
  gApplied = true;
  for (const auto & path : gFiles)
    if (!LoadMaterialPropertiesYaml(path, error)) return false;
  return true;
}

bool LoadMaterialPropertiesYaml(const std::string & path, std::string & error)
{
  YAML::Node doc;
  try {
    doc = YAML::LoadFile(path);
  }
  catch (const std::exception & e) {
    error = "cannot read " + path + ": " + e.what();
    return false;
  }
  if (!doc.IsSequence()) {
    error = path + ": expected a list of materials and surfaces at the top level";
    return false;
  }

  int nProperty = 0, nConst = 0;
  std::string source;

  // yaml-cpp throws on a value that is not the type asked for -- a string where a
  // number belongs, a scalar where a list belongs. Caught here so a malformed file
  // reports itself rather than terminating the process.
  try {

  for (const auto & entry : doc) {
    if (!CheckKeys(entry,
                   {"material", "surface", "provenance", "properties", "constants",
                    "birks_constant"},
                   path, error))
      return false;

    if (entry["provenance"] && entry["provenance"]["source"] && source.empty())
      source = entry["provenance"]["source"].as<std::string>();

    const bool isMaterial = (bool)entry["material"];
    const bool isSurface = (bool)entry["surface"];
    if (isMaterial == isSurface) {
      error = path + ": each entry needs exactly one of 'material' or 'surface'";
      return false;
    }
    const std::string name =
        isMaterial ? entry["material"].as<std::string>() : entry["surface"].as<std::string>();

    G4Material * material = isMaterial ? G4Material::GetMaterial(name, false) : nullptr;
    G4OpticalSurface * surface = isSurface ? FindSurface(name) : nullptr;
    if (isMaterial && material == nullptr) {
      error = path + ": no material named '" + name +
              "' -- it has to be built before the file is applied";
      return false;
    }
    if (isSurface && surface == nullptr) {
      error = path + ": no optical surface named '" + name +
              "' -- it has to be created before the file is applied";
      return false;
    }

    // Add to whatever table is already there rather than replacing it, so a file
    // can carry only the properties it wants to override.
    G4MaterialPropertiesTable * mpt =
        material ? material->GetMaterialPropertiesTable() : surface->GetMaterialPropertiesTable();
    if (mpt == nullptr) {
      mpt = new G4MaterialPropertiesTable();
      if (material) material->SetMaterialPropertiesTable(mpt);
      else surface->SetMaterialPropertiesTable(mpt);
    }

    if (entry["properties"]) {
      for (const auto & kv : entry["properties"]) {
        const std::string key = kv.first.as<std::string>();
        if (!ReadProperty(key, kv.second, mpt, name, error)) return false;
        nProperty++;
      }
    }

    if (entry["constants"]) {
      for (const auto & kv : entry["constants"]) {
        const std::string key = kv.first.as<std::string>();
        const auto & constNames = mpt->GetMaterialConstPropertyNames();
        const bool known =
            std::find(constNames.begin(), constNames.end(), key) != constNames.end();
        G4double value = 0.;
        std::string what;
        if (!ParseScalar(kv.second, value, what)) {
          error = name + "/" + key + ": " + what;
          return false;
        }
        if (!known) {
          error = name + ": '" + key + "' is not a Geant4 const-property name";
          return false;
        }
        mpt->AddConstProperty(key, value);
        nConst++;
      }
    }

    if (entry["birks_constant"]) {
      if (!isMaterial) {
        error = path + ": birks_constant belongs to a material, not to surface '" + name + "'";
        return false;
      }
      G4double kB = 0.;
      std::string what;
      if (!ParseBirks(entry["birks_constant"].as<std::string>(), material, kB, what)) {
        error = name + ": " + what;
        return false;
      }
      material->GetIonisation()->SetBirksConstant(kB);
    }
  }

  }
  catch (const std::exception & e) {
    error = path + ": " + e.what();
    return false;
  }

  gDigest = FileDigest(path);
  G4cout << "Material properties: " << path << " -- " << nProperty << " tables, " << nConst
         << " constants, digest " << gDigest << G4endl;
  if (!source.empty()) G4cout << "  source: " << source << G4endl;
  return true;
}
