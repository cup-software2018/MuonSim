#include <sstream>

#include "G4Event.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4RotationMatrix.hh"
#include "G4SystemOfUnits.hh"
#include "G4ios.hh"
#include "Randomize.hh"

#include "EventFileGen.hh"

const std::string & EventFileGen::Key()
{
  static const std::string key = "file";
  return key;
}

bool EventFileGen::LoadFile(const std::string & path, std::string & error)
{
  if (!LoadEventFile(path, fData, fUnits, error)) return false;

  // Start somewhere random. Every array task reads the same file, and starting all
  // of them at row zero would give one event set many times over rather than a
  // sample of the file.
  fCursor = (std::size_t)(G4UniformRand() * fData.NEvents());
  if (fCursor >= fData.NEvents()) fCursor = 0;

  // The units go in the log, defaults marked. A converter that wrote microseconds
  // and forgot to say so is then visible here rather than only in the answer.
  G4cout << "EventFileGen: " << fData.NEvents() << " events, " << fData.particles.size()
         << " particles from " << path << G4endl;
  G4cout << "EventFileGen: energy=" << fUnits.energy << ", time=" << fUnits.time
         << (fUnits.timeWasDefaulted ? " (DEFAULTED, not declared)" : "")
         << (fUnits.hasTimeColumn ? "" : " -- no t column, all particles at 0")
         << (fUnits.hasPolarization ? ", polarized" : "") << "; starting at event " << fCursor
         << G4endl;
  return true;
}

namespace {

// Uniform on SO(3): phi and psi uniform in [0, 2pi), and cos(theta) uniform in
// [-1, 1). Drawing theta uniformly instead would bunch orientations at the poles.
G4RotationMatrix RandomRotation()
{
  const G4double phi = CLHEP::twopi * G4UniformRand();
  const G4double theta = std::acos(2. * G4UniformRand() - 1.);
  const G4double psi = CLHEP::twopi * G4UniformRand();
  G4RotationMatrix r;
  r.set(phi, theta, psi);
  return r;
}

} // namespace

void EventFileGen::EmitParticles(G4PrimaryVertex *, const PosDir &) const {}

void EventFileGen::GenerateVertex(G4Event * event) const
{
  if (fData.Empty() || event == nullptr || !HasPosGen()) return;

  // Once per event, and shared by every vertex it needs: the particles of one file
  // event happen in one place, at different times.
  PosDir where = PosGen()->Generate();
  CheckInsideWorld(where.position, GetKey());

  const std::size_t index = fCursor;
  fCursor = (fCursor + 1) % fData.NEvents();

  const G4RotationMatrix rotation = fRotate ? RandomRotation() : G4RotationMatrix();

  G4PrimaryVertex * vertex = nullptr;
  G4double openAt = 0.;

  for (std::size_t i = fData.start[index]; i < fData.start[index + 1]; ++i) {
    const EventFileParticle & p = fData.particles[i];

    // A new vertex whenever the time moves on. GetTime() is the offset the
    // generator was given for this event -- the Poisson gap when decays pile up --
    // so a file's internal times sit on top of it rather than replacing it.
    if (vertex == nullptr || p.time != openAt) {
      vertex = new G4PrimaryVertex(where.position, GetTime() + p.time);
      event->AddPrimaryVertex(vertex);
      openAt = p.time;
    }

    auto * primary = new G4PrimaryParticle(p.definition);
    primary->SetMomentumDirection(fRotate ? rotation * p.direction : p.direction);
    primary->SetKineticEnergy(p.energy);
    if (p.polarized)
      primary->SetPolarization(fRotate ? rotation * p.polarization : p.polarization);
    vertex->SetPrimary(primary); // appends, so simultaneous particles share a vertex
  }
}
