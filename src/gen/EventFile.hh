#pragma once

#include <string>
#include <vector>

#include "G4ThreeVector.hh"
#include "G4Types.hh"

class G4ParticleDefinition;

// Reads a list of ready-made events from a text file.
//
// The format is deliberately dull, because its job is to be written by a script
// converting somebody else's generator -- DECAY0 for double beta decay above all --
// rather than by hand. Two header lines are REQUIRED and everything after them is
// data:
//
//   # units: energy=MeV time=ns
//   # columns: event pdg ke dx dy dz t polx poly polz
//   0   11   0.5230   0.1230 -0.4560  0.8810   0.0    0 0 0
//   0   11   1.2040  -0.7710  0.0120  0.6370   0.0    0 0 0
//   1   22   2.6145   0.0000  0.0000  1.0000   0.0    0 0 0
//
// Rows sharing an `event` value are one event, exactly as the Escape tree groups a
// decay's gammas. They must be consecutive.
//
// THE HEADER IS NOT OPTIONAL. A file of bare numbers is a file whose units are
// whatever the last person assumed, and this project has already been bitten once
// by a setting that was silently not applied. A missing header is an error.
//
//   energy=   REQUIRED: eV, keV, MeV or GeV. There is no sensible default -- keV
//             and MeV are both ordinary and differ by a thousand.
//   time=     optional, ns if absent, which is Geant4's own internal unit. The
//             units actually used are printed when the file loads, so a converter
//             that writes microseconds and forgets to say so is visible in the log
//             rather than only in the answer.
//
// COLUMNS, in this order. The first six are required, the rest optional:
//
//   event particle ke dx dy dz   the event id, the particle, kinetic energy, direction
//   t                         time within the event; 0 if the column is absent
//   polx poly polz            polarization; none if the columns are absent
//
// Positions are NOT in the format. Where an event happens comes from the position
// clause of /gen/vertex, which is what lets one converted file be used in any volume
// of any geometry.
//
// TIME COSTS A VERTEX. G4PrimaryParticle carries no laboratory time -- only
// G4PrimaryVertex does -- so particles at different times land on different
// vertices, at the same position. Within one event `t` must therefore be
// non-decreasing; an interleaved file is a converter bug and is rejected rather
// than quietly regrouped.
//
// `particle` takes either a PDG code or a Geant4 name. Codes because that is what a
// converter emits; names because "e-" is readable where 11 is not. Ions go as
// 10LZZZAAAI, so an alpha is 1000020040, or by name.
//
// ONE CODE IS NOT STANDARD. Geant4 gives the optical photon PDG -22, which the PDG
// itself does not define -- there is no optical photon in the standard, and -22
// there is the photon's own antiparticle. So -22 in a file from an outside generator
// probably does not mean what Geant4 will take it to mean. Write `opticalphoton`.

struct EventFileParticle {
  G4ParticleDefinition * definition = nullptr;
  G4double energy = 0.;        // kinetic, already in Geant4 units
  G4ThreeVector direction;     // normalised on read
  G4double time = 0.;          // within the event, already in Geant4 units
  G4ThreeVector polarization;  // zero when the file gives none
  bool polarized = false;
};

// Particles of every event, flat, with the events marked out by offsets:
// event i owns [start[i], start[i+1]). start has one more entry than there are
// events, so the last event needs no special case.
struct EventFileData {
  std::vector<EventFileParticle> particles;
  std::vector<std::size_t> start;

  std::size_t NEvents() const { return start.empty() ? 0 : start.size() - 1; }
  bool Empty() const { return NEvents() == 0; }
};

// What the header declared, for the log line: a default that was taken rather than
// stated is exactly what a reader needs to see.
struct EventFileUnits {
  std::string energy;
  std::string time;
  bool timeWasDefaulted = false;
  bool hasTimeColumn = false;
  bool hasPolarization = false;
};

// Returns false with a filled-in error -- naming the line -- on any problem, so the
// caller can refuse the run rather than proceed with half a file.
bool LoadEventFile(const std::string & path, EventFileData & out, EventFileUnits & units,
                   std::string & error);
