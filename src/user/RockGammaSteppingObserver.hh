#pragma once

#include <set>
#include <string>

#include "G4Types.hh"
#include "SteppingObserver.hh"

class RootManager;
class TTree;

// Records every gamma that escapes the rock into the hall, and kills it there.
//
// This is step one of a two-step rock-gamma calculation. U/Th/K decay inside Rock,
// and what matters is the flux crossing into the air: energy, position, direction.
// Step two re-injects that flux on the hall surface with
// "/gen/vertex rockgamma onvolume in Hall", which is far cheaper than tracking
// decays in every production run.
//
// Killing at the boundary is the point, not an optimisation: the gamma has been
// counted, and what it does next is exactly what step two simulates. It also means
// this observer must NOT be registered for an ordinary run -- it would remove rock
// gammas from the detectors it is meant to be feeding.
//
// USER CODE. It names volumes of this geometry, so it lives in src/user; the
// library only knows SteppingObserver.
class RockGammaSteppingObserver : public SteppingObserver {
public:
  // The defaults name this geometry's volumes: the rock shell, and the air it
  // wraps. One name each, since the rock is a uniform shell and the hall is one
  // volume -- the dome and the pit together.
  //
  // No filename: the Escape tree goes in the run's own output file, beside the
  // Event tree that holds the decay vertices. In from one, out through the other.
  explicit RockGammaSteppingObserver(std::set<std::string> fromVolumes = {"Rock"},
                                 std::set<std::string> toVolumes = {"Hall"});
  ~RockGammaSteppingObserver() override;

  std::string Describe() const override
  {
    return "records gammas crossing rock -> hall into an Escape tree, and KILLS them";
  }

  void BeginOfRun(const G4Run * run, RootManager * root) override;
  void EndOfRun(const G4Run * run, RootManager * root) override;
  void Step(const G4Step * step) override;

private:
  std::set<std::string> fFrom;
  std::set<std::string> fTo;

  // Not owned: the run's output file owns the tree.
  TTree * fTree = nullptr;

  // One row per escaping gamma. Flat branches: this is a list of crossings, not an
  // event structure, and it is read by an analysis macro rather than by anything
  // that needs the MC data model.
  float fEnergy = 0.f;                   // kinetic energy [MeV]
  float fX = 0.f, fY = 0.f, fZ = 0.f;    // where it crossed [mm]
  float fUx = 0.f, fUy = 0.f, fUz = 0.f; // unit direction
  float fTime = 0.f;                     // [ns]
  int fEventId = 0;

  long long fEscapes = 0;
};
