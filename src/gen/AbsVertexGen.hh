#pragma once

#include <memory>
#include <string>

#include <cmath>

#include "AbsPosGen.hh"
#include "G4ThreeVector.hh"
#include "G4Types.hh"
#include "Randomize.hh"

class G4Event;
class G4PrimaryVertex;

// Uniform on the unit sphere. Used by every source for particles whose direction
// the position generator did not dictate.
inline G4ThreeVector IsotropicDirection()
{
  const G4double cosTheta = 2. * G4UniformRand() - 1.;
  const G4double sinTheta = std::sqrt(1. - cosTheta * cosTheta);
  const G4double phi = 2. * M_PI * G4UniformRand();
  return G4ThreeVector(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);
}

// Produces one primary vertex per event.
//
// The base does the assembly every source needs -- sample the position, guard
// against starting outside the world, build the G4PrimaryVertex, add it to the
// event -- so a source only writes EmitParticles(): what comes out, and with
// what kinematics.
//
// The position generator is supplied by the user through the command, so any
// source composes with any position. When that position generator also dictates
// a direction (emitting into or out of a surface), it arrives as dirHint and the
// source is expected to honour it.
//
// CosmicMuonGen is the exception that overrides GenerateVertex: its direction is
// drawn jointly with the energy from the flux table and the position is derived
// from that direction, so it cannot come from a position generator at all.
class AbsVertexGen {
public:
  AbsVertexGen() = default;
  virtual ~AbsVertexGen() = default;

  virtual const std::string & GetKey() const = 0;

  virtual void GenerateVertex(G4Event * event) const;

  void SetPosGen(std::unique_ptr<AbsPosGen> posGen) { fPosGen = std::move(posGen); }
  bool HasPosGen() const { return fPosGen != nullptr; }
  AbsPosGen * PosGen() const { return fPosGen.get(); }

  // The vertex's time within the event. Zero for the decay that opens it; a later
  // offset for one that piles up on top, which is how two independent decays end up
  // in one event separated by their real gap.
  // A source that knows its own absolute rate in Hz says so here, and the
  // generator action drives the clock with it instead of /gen/activity. Zero means
  // "I have no idea", which is true of every radioactive source: its rate is the
  // sample's activity, which only the user knows. Cosmic muons are the opposite --
  // the flux table fixes the rate, and asking the user for it would invite a
  // number that disagrees with the histogram.
  virtual G4double GetRateHz() const { return 0.; }

  void SetTime(G4double time) { fTime = time; }
  G4double GetTime() const { return fTime; }

  void SetDescription(const std::string & description) { fDescription = description; }
  const std::string & GetDescription() const { return fDescription; }

protected:
  // dirHint.hasDirection tells the source whether the direction was dictated.
  virtual void EmitParticles(G4PrimaryVertex * vertex, const PosDir & dirHint) const = 0;

  // Aborts the run naming the offending coordinates if position is outside the
  // world -- Geant4 otherwise segfaults deep in the navigator. Also catches the
  // origin fallback a position generator returns for an unknown volume.
  void CheckInsideWorld(const G4ThreeVector & position, const std::string & who) const;

private:
  std::unique_ptr<AbsPosGen> fPosGen;
  G4double fTime = 0.;
  std::string fDescription;
};
