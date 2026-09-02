#pragma once

#include "StackingObserver.hh"

// Keeps the gammas -- and what makes them -- and throws away everything else,
// before any of it is tracked.
//
// Measured, 200000 40K decays: 1260504 tracks offered, 65% of them electrons and
// neutrinos. The gammas are the measurement; the rest is cost. 70 s becomes 5.5 s,
// and for 232Th -- a chain, so far more of everything -- 177 s becomes 45 s.
//
// TWO THINGS MUST SURVIVE, and both were found by killing them and watching the
// escaping gammas go to zero:
//
//   - The PRIMARY, because the radioactive parent IS the primary track. Kill it and
//     nothing decays at all.
//   - Every ION, because in Geant4 a decay gamma is emitted by the DAUGHTER, not by
//     the parent: 40K decays to a secondary 40Ar* track, and the 1461 keV gamma comes
//     out of that track's de-excitation. Killing secondary ions gave 0 escaping
//     gammas where 5262 were expected. In a chain the same rule keeps each daughter,
//     which is the next link, so 238U and 232Th run through.
//
// KILLING THE REST DOES NOT CHANGE GAMMA TRANSPORT. Compton scattering still happens
// and the gamma carries on with less energy; photoelectric absorption still removes
// it. Only the recoil electron's own track is discarded. What is lost is a beta's
// bremsstrahlung, and the measurement puts all of it BELOW 100 keV:
//
//               escaping gammas, filter on / filter off
//     band        40K (200k decays)      232Th (20k decays)
//     < 50 keV    0.100 +- 0.022          0.89 +- 0.08
//     50-100 keV  0.692 +- 0.035          1.00 +- 0.02
//     0.1-3 MeV   every band within 2 sigma of 1, in both
//     1461 line   1.054 +- 0.153                --
//
// 40K loses its soft end and 232Th does not, because 40K has one real gamma and its
// 1.31 MeV beta, so bremsstrahlung is most of what escapes soft; the Th chain emits
// enough real gammas to bury it. Either way 100 keV is nowhere near enough to cross
// the 250 mm of lead around the crystals, so nothing this study looks at is affected.
//
// NEUTRONS ARE KILLED TOO, so their capture gammas are absent from the Escape tree.
// One neutron in 20000 232Th decays, so it costs nothing here; the (alpha,n) yield of
// U/Th in rock is a background in its own right and is measured as one, with this
// observer off.
//
// USER CODE, and a StackingObserver rather than a Geant4 stacking action: the library's
// StackingAction asks it. Off unless /observer/stack/enable rockgamma.
class RockGammaStackingObserver : public StackingObserver {
public:
  RockGammaStackingObserver();

  std::string Describe() const override;

  bool Classify(const G4Track * track, G4ClassificationOfNewTrack & classification) override;

  void EndOfRun(const G4Run * run, RootManager * root) override;

private:
  long long fKeptPrimaries = 0;
  long long fKeptGammas = 0;
  long long fKeptIons = 0;
  long long fKilledNeutrons = 0;
  long long fKilledOther = 0;
};
