#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "AbsEnergyGen.hh"
#include "AbsPosGen.hh"
#include "AbsVertexGen.hh"
#include "AmBeGen.hh"
#include "CfGen.hh"
#include "CosmicMuonGen.hh"
#include "DataPath.hh"
#include "EnergyGenerators.hh"
#include "G4Exception.hh"
#include "G4IonTable.hh"
#include "G4NistManager.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4UnitsTable.hh"
#include "IBDGen.hh"
#include "ParticleVertexGen.hh"
#include "PositionGenerators.hh"
#include "EventFileGen.hh"
#include "RockGammaGen.hh"
#include "VertexGenBuilder.hh"

namespace
{
std::string ToLower(const std::string & s)
{
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return out;
}

std::vector<std::string> Tokenize(const std::string & line)
{
  std::vector<std::string> tokens;
  std::istringstream iss(line);
  std::string token;
  while (iss >> token)
    tokens.push_back(token);
  return tokens;
}

// True only if the whole token parses as a number (so "60Co" is not a number).
bool AsNumber(const std::string & token, G4double & value)
{
  if (token.empty()) return false;
  char * end = nullptr;
  value = std::strtod(token.c_str(), &end);
  return end != nullptr && *end == '\0';
}

bool IsUnitOf(const std::string & token, const G4String & category)
{
  for (auto * cat : G4UnitDefinition::GetUnitsTable()) {
    if (cat == nullptr || cat->GetName() != category) continue;
    for (auto * unit : cat->GetUnitsList())
      if (unit != nullptr && (token == unit->GetName() || token == unit->GetSymbol())) return true;
  }
  return false;
}

G4double ReadOptionalUnit(const std::vector<std::string> & tokens, std::size_t & i,
                          const G4String & category, G4double defaultUnit)
{
  if (i < tokens.size() && IsUnitOf(tokens[i], category)) {
    const G4double unit = G4UnitDefinition::GetValueOf(tokens[i]);
    i++;
    return unit;
  }
  return defaultUnit;
}

// Consumes the kinetic-energy slot at tokens[i], advancing i.
//
// A leading number is always the energy value (never the first component of a
// direction vector), so a direction can only be given together with a k.e.
// Anything else must be a distribution keyword.
//
//   <value> [unit]                            fixed
//   uniform  <min> <max> [unit]
//   gauss    <mean> <sigma> [unit]
//   exp      <E0> [unit]
//   powerlaw <index> <min> <max> [unit]       index is dimensionless
//   spectrum <E>:<w> <E>:<w> ... [unit]       ':' marks each point unambiguously
//
// Returns nullptr with an empty error when there is no energy spec here (the
// caller then uses 0 = at rest, and the token is left for the clause parser), or
// nullptr with a message when a distribution keyword is present but malformed.
std::unique_ptr<AbsEnergyGen> ParseEnergySlot(const std::vector<std::string> & tokens,
                                             std::size_t & i, std::string & error)
{
  if (i >= tokens.size()) return nullptr; // absent

  G4double value = 0.;
  if (AsNumber(tokens[i], value)) {
    i++;
    return std::make_unique<FixedEnergyGen>(value * ReadOptionalUnit(tokens, i, "Energy", MeV));
  }

  const std::string dist = ToLower(tokens[i]);

  // Reads n numeric arguments for the named distribution.
  auto readNumbers = [&](int n, G4double * out) {
    if (tokens.size() < i + 1 + (std::size_t)n) {
      error = "'" + dist + "' needs " + std::to_string(n) + " numeric argument(s)";
      return false;
    }
    for (int k = 0; k < n; k++) {
      if (!AsNumber(tokens[i + 1 + k], out[k])) {
        error = "'" + dist + "' argument is not a number: " + tokens[i + 1 + k];
        return false;
      }
    }
    i += 1 + (std::size_t)n;
    return true;
  };

  if (dist == "uniform" || dist == "gauss") {
    G4double a[2];
    if (!readNumbers(2, a)) return nullptr;
    const G4double unit = ReadOptionalUnit(tokens, i, "Energy", MeV);
    if (dist == "uniform") {
      if (a[1] < a[0]) {
        error = "'uniform' max is below min";
        return nullptr;
      }
      return std::make_unique<UniformEnergyGen>(a[0] * unit, a[1] * unit);
    }
    if (a[1] < 0.) {
      error = "'gauss' sigma is negative";
      return nullptr;
    }
    return std::make_unique<GaussEnergyGen>(a[0] * unit, a[1] * unit);
  }

  if (dist == "exp") {
    G4double e0 = 0.;
    if (!readNumbers(1, &e0)) return nullptr;
    if (e0 <= 0.) {
      error = "'exp' E0 must be positive";
      return nullptr;
    }
    return std::make_unique<ExpEnergyGen>(e0 * ReadOptionalUnit(tokens, i, "Energy", MeV));
  }

  if (dist == "powerlaw") {
    G4double a[3]; // index, min, max
    if (!readNumbers(3, a)) return nullptr;
    const G4double unit = ReadOptionalUnit(tokens, i, "Energy", MeV);
    if (a[1] <= 0. || a[2] < a[1]) {
      error = "'powerlaw' needs 0 < min <= max";
      return nullptr;
    }
    return std::make_unique<PowerLawEnergyGen>(a[0], a[1] * unit, a[2] * unit);
  }

  if (dist == "spectrum") {
    std::vector<G4double> energies, weights;
    std::size_t k = i + 1;
    for (; k < tokens.size(); k++) {
      const std::size_t colon = tokens[k].find(':');
      if (colon == std::string::npos) break;
      G4double e = 0., w = 0.;
      if (!AsNumber(tokens[k].substr(0, colon), e) ||
          !AsNumber(tokens[k].substr(colon + 1), w)) {
        error = "'spectrum' point is not <energy>:<weight>: " + tokens[k];
        return nullptr;
      }
      if (w < 0.) {
        error = "'spectrum' weight is negative: " + tokens[k];
        return nullptr;
      }
      energies.push_back(e);
      weights.push_back(w);
    }
    if (energies.empty()) {
      error = "'spectrum' needs at least one <energy>:<weight> point";
      return nullptr;
    }
    i = k;
    const G4double unit = ReadOptionalUnit(tokens, i, "Energy", MeV);
    for (auto & e : energies)
      e *= unit;
    return std::make_unique<SpectrumEnergyGen>(energies, weights);
  }

  // Not a number and not a distribution keyword: there is no energy spec here,
  // the token belongs to the clause list. Leave i where it is and say nothing --
  // an actual typo surfaces as "unknown clause" further on.
  return nullptr;
}

// Standard particle name (e-, mu-, gamma, ...) or an isotope in either the
// physics convention ("60Co") or Geant4's own ion name ("Co60").
G4ParticleDefinition * ResolveParticle(const std::string & name)
{
  if (auto * particle = G4ParticleTable::GetParticleTable()->FindParticle(name)) return particle;

  G4int A = 0;
  std::string symbol;

  std::size_t lead = 0;
  while (lead < name.size() && std::isdigit((unsigned char)name[lead]))
    lead++;

  if (lead > 0 && lead < name.size()) { // "60Co"
    A = std::atoi(name.substr(0, lead).c_str());
    symbol = name.substr(lead);
  }
  else { // "Co60"
    std::size_t trail = name.size();
    while (trail > 0 && std::isdigit((unsigned char)name[trail - 1]))
      trail--;
    if (trail > 0 && trail < name.size()) {
      symbol = name.substr(0, trail);
      A = std::atoi(name.substr(trail).c_str());
    }
  }

  if (A <= 0 || symbol.empty()) return nullptr;

  const G4int Z = G4NistManager::Instance()->GetZ(symbol);
  if (Z <= 0) return nullptr;

  return G4IonTable::GetIonTable()->GetIon(Z, A);
}
} // namespace

std::string SourceTypeNames()
{
  return CosmicMuonGen::Key() + ", " + RockGammaGen::Key() + ", " + IBDGen::Key() + ", " +
         AmBeGen::Key() + ", " + CfGen::Key() + ", " + EventFileGen::Key();
}

std::unique_ptr<AbsVertexGen> BuildVertexGen(const std::string & line)
{
  const auto tokens = Tokenize(line);
  std::size_t i = 0;

  auto fail = [&line](const char * code, const std::string & why) {
    G4Exception("VertexGenBuilder", code, FatalException,
                (why + "\n  in: /gen/vertex " + line).c_str());
  };

  if (tokens.empty()) {
    fail("GEN610", "empty /gen/vertex command");
    return nullptr;
  }

  // ---- type ----
  const std::string type = tokens[i++];
  const std::string typeLower = ToLower(type);

  const bool isCosmic = (typeLower == ToLower(CosmicMuonGen::Key()));
  const bool isRockGamma = (typeLower == ToLower(RockGammaGen::Key()));
  const bool isIBD = (typeLower == ToLower(IBDGen::Key()));
  const bool isAmBe = (typeLower == ToLower(AmBeGen::Key()));
  const bool isCf = (typeLower == ToLower(CfGen::Key()));
  const bool isFile = (typeLower == ToLower(EventFileGen::Key()));
  const bool isSource = isCosmic || isRockGamma || isIBD || isAmBe || isCf || isFile;

  G4ParticleDefinition * particle = nullptr;
  if (!isSource) {
    particle = ResolveParticle(type);
    if (particle == nullptr) {
      fail("GEN617", "unknown type '" + type +
                         "' -- expected a Geant4 particle name, an isotope such as 60Co, "
                         "or a source (" + SourceTypeNames() + ")");
      return nullptr;
    }
  }

  // ---- optional energy spec, directly after the type ----
  std::string energyError;
  auto energyGen = ParseEnergySlot(tokens, i, energyError);
  if (!energyError.empty()) {
    fail("GEN619", energyError);
    return nullptr;
  }
  if (energyGen && isSource) {
    fail("GEN618", "source '" + type + "' sets its own energies, so it takes no energy spec");
    return nullptr;
  }

  // ---- clauses, any order ----
  std::unique_ptr<AbsPosGen> posGen;
  std::string posClause;
  OnVolumePosGen::Side side = OnVolumePosGen::Side::Both;
  bool haveLaw = false;
  AngularLaw law = AngularLaw::Isotropic;
  bool randomDir = false;
  bool rotateEvents = false;
  bool haveDir = false;
  G4ThreeVector direction;
  bool havePol = false;
  bool haveSurface = false;
  G4ThreeVector surfaceCentre;
  G4double surfaceRadius = 0.;
  G4ThreeVector polarization;
  G4double time = 0.;
  std::string inputFile;

  auto readVector = [&](const char * what, G4ThreeVector & out) {
    if (tokens.size() < i + 3) {
      fail("GEN611", std::string(what) + " needs three components");
      return false;
    }
    G4double v[3] = {0., 0., 0.};
    for (int k = 0; k < 3; k++) {
      if (!AsNumber(tokens[i + k], v[k])) {
        fail("GEN612", std::string(what) + " component is not a number: " + tokens[i + k]);
        return false;
      }
    }
    i += 3;
    out = G4ThreeVector(v[0], v[1], v[2]);
    return true;
  };

  auto needName = [&](const char * what, std::string & out) {
    if (i >= tokens.size()) {
      fail("GEN613", std::string(what) + " needs a name");
      return false;
    }
    out = tokens[i++];
    return true;
  };

  while (i < tokens.size()) {
    const std::string clause = ToLower(tokens[i++]);

    if (clause == "sphere") {
      // sphere <x> <y> <z> <R> [unit] -- the virtual surface cosmic muons are
      // launched from. Centre and radius share one unit.
      G4ThreeVector xyz;
      if (!readVector("'sphere'", xyz)) return nullptr;
      if (i >= tokens.size()) {
        fail("GEN625", "'sphere' needs <x> <y> <z> <radius> [unit]");
        return nullptr;
      }
      surfaceRadius = std::atof(tokens[i++].c_str());
      const G4double lengthUnit = ReadOptionalUnit(tokens, i, "Length", mm);
      surfaceCentre = xyz * lengthUnit;
      surfaceRadius *= lengthUnit;
      if (surfaceRadius <= 0.) {
        fail("GEN626", "'sphere' radius must be positive");
        return nullptr;
      }
      haveSurface = true;
    }
    else if (clause == "point") {
      G4ThreeVector xyz;
      if (!readVector("'point'", xyz)) return nullptr;
      xyz *= ReadOptionalUnit(tokens, i, "Length", mm);
      posGen = std::make_unique<PointPosGen>(xyz);
      posClause = clause;
    }
    else if (clause == "involume" || clause == "multivolume") {
      std::string name;
      if (!needName(("'" + clause + "'").c_str(), name)) return nullptr;
      if (clause == "involume") posGen = std::make_unique<InVolumePosGen>(name);
      else posGen = std::make_unique<MultiVolumePosGen>(name);
      posClause = clause;
    }
    else if (clause == "onvolume") {
      // Optional side comes before the volume name: onvolume in World
      if (i < tokens.size()) {
        bool ok = false;
        const auto parsed = OnVolumePosGen::ParseSide(tokens[i], ok);
        if (ok) {
          side = parsed;
          i++;
        }
      }
      std::string name;
      if (!needName("'onvolume'", name)) return nullptr;
      posGen = std::make_unique<OnVolumePosGen>(name);
      posClause = clause;
    }
    else if (clause == "random") { randomDir = true; }
    // Only 'file' looks at this; it is rejected below for anything else rather than
    // being accepted and ignored.
    else if (clause == "rotate") { rotateEvents = true; }
    else if (clause == "cos" || clause == "iso") {
      haveLaw = true;
      law = (clause == "cos") ? AngularLaw::Cosine : AngularLaw::Isotropic;
    }
    else if (clause == "direction") {
      if (!readVector("'direction'", direction)) return nullptr;
      haveDir = true;
    }
    else if (clause == "polarization" || clause == "polarisation") {
      if (!readVector("'polarization'", polarization)) return nullptr;
      havePol = true;
    }
    else if (clause == "time") {
      G4double t = 0.;
      if (i >= tokens.size() || !AsNumber(tokens[i], t)) {
        fail("GEN612", "'time' needs a number");
        return nullptr;
      }
      i++;
      time = t * ReadOptionalUnit(tokens, i, "Time", ns);
    }
    else if (clause == "input") {
      if (i >= tokens.size()) {
        fail("GEN623", "'input' needs a file name");
        return nullptr;
      }
      // Through the same four-step lookup -m uses -- as given, $MUONSIM_DATA,
      // beside the executable, the installed data directory -- so a macro can
      // name a data file by a bare relative path and still be run from any
      // directory. Without this the path was resolved against the process's
      // working directory, which made a macro only usable from one place.
      //
      // Falls back to the token as typed when nothing is found, so the error
      // downstream names the file the user actually asked for.
      const std::string resolved = ResolveDataFile(tokens[i]);
      inputFile = resolved.empty() ? tokens[i] : resolved;
      i++;
    }
    else {
      fail("GEN614", "unknown clause '" + tokens[i - 1] +
                         "' -- expected point | involume | onvolume | multivolume | random | "
                         "direction | cos | iso | polarization | time | input | sphere | rotate");
      return nullptr;
    }
  }

  if (haveDir && randomDir) {
    fail("GEN615", "'direction' and 'random' contradict each other");
    return nullptr;
  }

  // A restricted surface dictates the direction, so an explicit one conflicts.
  if (haveDir && posClause == "onvolume" && side != OnVolumePosGen::Side::Both) {
    fail("GEN615", "'onvolume " + std::string(OnVolumePosGen::SideName(side)) +
                       "' already fixes the emission side, so 'direction' conflicts with it");
    return nullptr;
  }

  if (posClause == "onvolume") {
    auto * onVol = static_cast<OnVolumePosGen *>(posGen.get());
    onVol->SetSide(side);
    // A flux crossing a surface follows Lambert, so that is rockgamma's default;
    // surface activity decaying in place is isotropic, the default elsewhere.
    onVol->SetAngularLaw(haveLaw ? law : (isRockGamma ? AngularLaw::Cosine
                                                      : AngularLaw::Isotropic));
  }

  // ---- build the source ----
  // Rejected, not ignored: a clause that quietly does nothing is a run that looks
  // configured and is not.
  if (rotateEvents && !isFile) {
    fail("GEN628", "'rotate' turns each event of an event list to a random orientation, so "
                   "it only means something for '" + EventFileGen::Key() + "'");
    return nullptr;
  }

  // Only ParticleVertexGen applies it, so on any source it would do nothing --
  // and a clause that does nothing is a run that looks configured and is not.
  // 'file' carries its own per-particle polarization columns instead.
  if (havePol && isSource) {
    fail("GEN630", "'polarization' is applied to a single named particle; source '" + type +
                       "' sets its own, so the clause would do nothing here" +
                       (isFile ? ". Use the polx poly polz columns of the event file." : ""));
    return nullptr;
  }

  std::unique_ptr<AbsVertexGen> vertex;

  if (isCosmic) {
    if (inputFile.empty()) {
      fail("GEN624", "'" + CosmicMuonGen::Key() +
                         "' needs the SPHERE flux ROOT file, holding h_flux: input <flux.root>");
      return nullptr;
    }
    auto gen = std::make_unique<CosmicMuonGen>(inputFile);
    if (!gen->HasFlux()) {
      fail("GEN624", "no usable h_flux histogram in " + inputFile);
      return nullptr;
    }
    if (haveSurface) gen->SetSurface(surfaceCentre, surfaceRadius);
    vertex = std::move(gen); // derives its own position, so no posGen needed
  }
  else if (isRockGamma) {
    if (inputFile.empty()) {
      fail("GEN623", "'" + RockGammaGen::Key() +
                         "' needs a gamma spectrum file: input <spectrum.yml>");
      return nullptr;
    }
    auto gen = std::make_unique<RockGammaGen>();
    std::string loadError;
    if (!gen->LoadSpectrumFile(inputFile, loadError)) {
      fail("GEN623", loadError);
      return nullptr;
    }
    vertex = std::move(gen);
  }
  else if (isIBD) {
    // Required, not optional: there is no single canonical antineutrino
    // spectrum (reactor, geoneutrino, ... all differ), so falling back to a
    // built-in one would quietly produce a non-physical source.
    if (inputFile.empty()) {
      fail("GEN623", "'" + IBDGen::Key() +
                         "' needs an antineutrino spectrum file -- there is no single standard "
                         "one: input <spectrum.yml>");
      return nullptr;
    }
    auto gen = std::make_unique<IBDGen>();
    std::string loadError;
    if (!gen->LoadSpectrumFile(inputFile, loadError)) {
      fail("GEN623", loadError);
      return nullptr;
    }
    vertex = std::move(gen);
  }
  else if (isAmBe) {
    auto gen = std::make_unique<AmBeGen>();
    if (!inputFile.empty()) { // otherwise the built-in measured table is used
      std::string loadError;
      if (!gen->LoadSpectrumFile(inputFile, loadError)) {
        fail("GEN623", loadError);
        return nullptr;
      }
    }
    vertex = std::move(gen);
  }
  else if (isFile) {
    if (inputFile.empty()) {
      fail("GEN623", "'" + EventFileGen::Key() +
                         "' needs an event list: input <events.txt>");
      return nullptr;
    }
    // The file gives directions; onvolume dictates one of its own. Rejected here
    // rather than per event, and rejected rather than silently letting one win.
    if (posClause == "onvolume") {
      fail("GEN629", "'" + EventFileGen::Key() +
                         "' carries its own directions, so it cannot be combined with "
                         "'onvolume', which imposes one. Use point, involume or "
                         "multivolume.");
      return nullptr;
    }
    auto gen = std::make_unique<EventFileGen>();
    std::string loadError;
    if (!gen->LoadFile(inputFile, loadError)) {
      fail("GEN623", loadError);
      return nullptr;
    }
    gen->SetRotate(rotateEvents);
    vertex = std::move(gen);
  }
  else if (isCf) {
    if (!inputFile.empty()) {
      fail("GEN623", "'" + CfGen::Key() +
                         "' has three built-in distributions (neutron energy, gamma multiplicity, "
                         "gamma energy), which one 'input' file cannot express");
      return nullptr;
    }
    vertex = std::make_unique<CfGen>();
  }
  else {
    if (!energyGen) energyGen = std::make_unique<FixedEnergyGen>(0.); // at rest
    auto gen = std::make_unique<ParticleVertexGen>(particle, std::move(energyGen));
    if (haveDir) gen->SetDirection(direction);
    if (havePol) gen->SetPolarization(polarization);
    vertex = std::move(gen);
  }

  if (haveSurface && !isCosmic) {
    fail("GEN627", "'sphere' only applies to '" + CosmicMuonGen::Key() +
                       "', which is the only source that launches from a surface of its own");
    return nullptr;
  }

  if (!isCosmic) {
    if (!posGen) {
      fail("GEN616", "no position given -- add one of: point | involume | onvolume | multivolume");
      return nullptr;
    }
    vertex->SetPosGen(std::move(posGen));
  }
  else if (posGen) {
    fail("GEN616", "'" + CosmicMuonGen::Key() +
                       "' derives its own position from the sampled direction, so a position "
                       "clause conflicts with it");
    return nullptr;
  }

  vertex->SetTime(time);
  vertex->SetDescription(line);
  return vertex;
}
