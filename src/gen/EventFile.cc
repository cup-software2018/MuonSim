#include <cmath>
#include <fstream>
#include <sstream>

#include "G4IonTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4SystemOfUnits.hh"

#include "EventFile.hh"

namespace {

std::string Trim(const std::string & s)
{
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return {};
  const auto e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

// Named in the header, never guessed: a typo has to be an error, not MeV.
bool EnergyUnit(const std::string & name, G4double & unit)
{
  if (name == "eV") { unit = eV; return true; }
  if (name == "keV") { unit = keV; return true; }
  if (name == "MeV") { unit = MeV; return true; }
  if (name == "GeV") { unit = GeV; return true; }
  return false;
}

bool TimeUnit(const std::string & name, G4double & unit)
{
  if (name == "ps") { unit = picosecond; return true; }
  if (name == "ns") { unit = nanosecond; return true; }
  if (name == "us") { unit = microsecond; return true; }
  if (name == "ms") { unit = millisecond; return true; }
  if (name == "s") { unit = second; return true; }
  return false;
}

// A PDG code or a Geant4 name, whichever the field holds.
//
// Both, because neither alone is enough. A converter emits codes; but an OPTICAL
// PHOTON has PDG 0, which is also what "no code" looks like, so it can only be
// named -- and optical photons are the very case polarization exists for. Names
// also make a hand-written file readable: "e-" instead of 11.
G4ParticleDefinition * Resolve(const std::string & field)
{
  const bool numeric =
      !field.empty() &&
      field.find_first_not_of("+-0123456789") == std::string::npos &&
      field.find_first_of("0123456789") != std::string::npos;

  if (!numeric) return G4ParticleTable::GetParticleTable()->FindParticle(field);

  const G4int pdg = std::stoi(field);
  if (auto * p = G4ParticleTable::GetParticleTable()->FindParticle(pdg)) return p;

  // Ions carry 10LZZZAAAI and have to come from the ion table, which is why an
  // alpha written as 1000020040 works and would otherwise come back null.
  const G4int abs = std::abs(pdg);
  if (abs < 1000000000) return nullptr;

  const G4int Z = (abs / 10000) % 1000;
  const G4int A = (abs / 10) % 1000;
  const G4int lvl = abs % 10;
  if (Z <= 0 || A <= 0) return nullptr;

  return G4IonTable::GetIonTable()->GetIon(Z, A, lvl);
}

} // namespace

bool LoadEventFile(const std::string & path, EventFileData & out, EventFileUnits & units,
                   std::string & error)
{
  out.particles.clear();
  out.start.clear();
  units = EventFileUnits{};

  std::ifstream in(path);
  if (!in) {
    error = "cannot open event file: " + path;
    return false;
  }

  G4double energyUnit = 0., timeUnit = nanosecond;
  bool haveUnits = false, haveColumns = false;

  std::string line;
  long long lineNo = 0;
  long long currentEvent = 0;
  bool inEvent = false;
  G4double lastTime = 0.;

  auto at = [&](const std::string & what) {
    std::ostringstream msg;
    msg << path << ":" << lineNo << ": " << what;
    return msg.str();
  };

  while (std::getline(in, line)) {
    lineNo++;
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) continue;

    if (trimmed[0] == '#') {
      // Header lines are comments to anything else that reads the file, which is
      // what lets a converter emit them without a second format.
      std::istringstream hs(trimmed.substr(1));
      std::string key;
      hs >> key;

      if (key == "units:") {
        std::string token;
        while (hs >> token) {
          const auto eq = token.find('=');
          if (eq == std::string::npos) {
            error = at("units must be written name=unit, e.g. energy=MeV time=ns");
            return false;
          }
          const std::string name = token.substr(0, eq), value = token.substr(eq + 1);
          if (name == "energy") {
            if (!EnergyUnit(value, energyUnit)) {
              error = at("unknown energy unit '" + value + "' -- eV, keV, MeV or GeV");
              return false;
            }
            units.energy = value;
          }
          else if (name == "time") {
            if (!TimeUnit(value, timeUnit)) {
              error = at("unknown time unit '" + value + "' -- ps, ns, us, ms or s");
              return false;
            }
            units.time = value;
          }
          else {
            error = at("unknown unit '" + name + "' -- expected energy or time");
            return false;
          }
        }
        if (units.energy.empty()) {
          error = at("'energy=' is required -- keV and MeV are both ordinary and differ "
                     "by a thousand, so there is no default");
          return false;
        }
        if (units.time.empty()) {
          units.time = "ns";
          units.timeWasDefaulted = true;
        }
        haveUnits = true;
      }
      else if (key == "columns:") {
        std::vector<std::string> cols;
        std::string c;
        while (hs >> c) cols.push_back(c);

        const std::vector<std::string> required = {"event", "particle", "ke", "dx", "dy", "dz"};
        if (cols.size() < required.size() ||
            !std::equal(required.begin(), required.end(), cols.begin())) {
          error = at("the first six columns must be: event particle ke dx dy dz");
          return false;
        }
        // Optional tails, in this order and no other. A free order would let two
        // files look alike and mean different things.
        std::size_t i = required.size();
        if (i < cols.size() && cols[i] == "t") { units.hasTimeColumn = true; i++; }
        if (i + 2 < cols.size() && cols[i] == "polx" && cols[i + 1] == "poly" &&
            cols[i + 2] == "polz") {
          units.hasPolarization = true;
          i += 3;
        }
        if (i != cols.size()) {
          error = at("unexpected column '" + cols[i] +
                     "' -- after event particle ke dx dy dz come, optionally, t then "
                     "polx poly polz");
          return false;
        }
        haveColumns = true;
      }
      continue;
    }

    if (!haveUnits || !haveColumns) {
      error = at("data before the header -- both '# units:' and '# columns:' must come first");
      return false;
    }

    long long ev = 0;
    std::string what;
    G4double ke = 0., dx = 0., dy = 0., dz = 0.;
    std::istringstream ds(trimmed);
    if (!(ds >> ev >> what >> ke >> dx >> dy >> dz)) {
      error = at("expected at least 6 fields: event particle ke dx dy dz");
      return false;
    }

    EventFileParticle p;
    p.definition = Resolve(what);
    if (p.definition == nullptr) {
      error = at("no particle for '" + what +
                 "' -- expected a PDG code, an ion as 10LZZZAAAI, or a Geant4 name "
                 "such as e- or opticalphoton");
      return false;
    }
    if (ke < 0.) {
      error = at("negative kinetic energy");
      return false;
    }
    p.energy = ke * energyUnit;

    const G4double norm = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (norm <= 0.) {
      error = at("direction has zero length");
      return false;
    }
    p.direction = G4ThreeVector(dx / norm, dy / norm, dz / norm);

    if (units.hasTimeColumn) {
      G4double t = 0.;
      if (!(ds >> t)) {
        error = at("the header declares a 't' column but this row has none");
        return false;
      }
      if (t < 0.) {
        error = at("negative time");
        return false;
      }
      p.time = t * timeUnit;
    }

    if (units.hasPolarization) {
      G4double px = 0., py = 0., pz = 0.;
      if (!(ds >> px >> py >> pz)) {
        error = at("the header declares polarization columns but this row has none");
        return false;
      }
      p.polarization = G4ThreeVector(px, py, pz);
      p.polarized = (p.polarization.mag2() > 0.);
    }

    // A new event starts wherever the id changes. Consecutive is required rather
    // than sorted-on-read: a file whose events are interleaved is a converter bug,
    // and silently regrouping it would hide that.
    if (!inEvent || ev != currentEvent) {
      out.start.push_back(out.particles.size());
      currentEvent = ev;
      inEvent = true;
      lastTime = 0.;
    }
    // Non-decreasing within an event, because each distinct time opens a vertex and
    // going backwards would open a second vertex at a time already passed.
    if (p.time < lastTime) {
      error = at("time goes backwards within event " + std::to_string(ev) +
                 " -- rows of one event must be in time order");
      return false;
    }
    lastTime = p.time;

    out.particles.push_back(p);
  }

  if (!haveUnits || !haveColumns) {
    error = path + ": no header -- '# units:' and '# columns:' are required";
    return false;
  }
  if (out.particles.empty()) {
    error = path + ": header but no events";
    return false;
  }

  out.start.push_back(out.particles.size()); // closes the last event
  return true;
}
