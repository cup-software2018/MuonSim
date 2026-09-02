#pragma once

#include <string>

#include "AbsVertexGen.hh"
#include "EventFile.hh"
#include "G4Types.hh"

class G4Event;

class G4PrimaryVertex;

// Events read from a text file, one vertex each, however many particles.
//
// For output from a generator this code does not contain -- DECAY0 for double beta
// decay above all, and anything else that can be converted to the format in
// EventFile.hh. The file says WHAT came out of the decay; the /gen/vertex position
// clause says where it happened, so one DECAY0 file serves any volume of any
// geometry:
//
//   /gen/vertex file input decay0_2nubb.txt involume Crystal
//
// GenerateVertex is overridden, for one reason: TIME. G4PrimaryParticle carries no
// laboratory time, only G4PrimaryVertex does, so particles at different times need
// different vertices -- at the same position, which is why the position generator is
// still called, once, and its answer shared. With no `t` column that comes to a
// single vertex and the base's behaviour.
//
// THE FILE IS HELD IN MEMORY. A million particles is about 40 MB, so there is no
// streaming and no seek index, and picking an event is an array index. That is what
// makes a random starting offset cheap enough to be the default: without one, every
// task of an array job replays the file from the same end, which is duplication
// dressed as statistics.
class EventFileGen : public AbsVertexGen {
public:
  EventFileGen() = default;
  ~EventFileGen() override = default;

  static const std::string & Key();
  const std::string & GetKey() const override { return Key(); }

  bool LoadFile(const std::string & path, std::string & error);

  // Turn each event to a random orientation as it is used.
  //
  // OFF by default, because rotating changes the event and nothing in this project
  // changes the physics without being asked. Turn it on when a run needs more
  // events than the file holds: replaying an event verbatim repeats the angles
  // BETWEEN its particles, which no amount of fresh position or fresh transport
  // decorrelates. A decay is isotropic, so the rotation costs nothing physical --
  // but a file whose directions mean something (a beam, an aligned source) must
  // keep it off.
  void SetRotate(G4bool on) { fRotate = on; }
  G4bool GetRotate() const { return fRotate; }

  // The starting event is RANDOM, which is worth knowing before writing a test:
  // the order events come out in is not the order they sit in the file. Put several
  // particles in ONE file event if the order matters to what is being checked.
  std::size_t NEvents() const { return fData.NEvents(); }
  std::size_t NParticles() const { return fData.particles.size(); }

  void GenerateVertex(G4Event * event) const override;

protected:
  // Unused: GenerateVertex does the work, because one file event can need several
  // vertices. Kept because the base declares it pure.
  void EmitParticles(G4PrimaryVertex * vertex, const PosDir & where) const override;

private:
  EventFileData fData;
  EventFileUnits fUnits;
  mutable std::size_t fCursor = 0;
  G4bool fRotate = false;
};
