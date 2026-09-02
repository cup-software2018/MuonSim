#pragma once

#include <string>
#include <vector>

#include "G4Types.hh"

// Reads a tabulated energy spectrum from YAML:
//
//   energy_unit: MeV      # optional, default MeV
//   energy_min: 1.806     # optional sampling-window bounds
//   energy_max: 10.0
//   energy: [ ... ]       # required, same length as flux
//   flux:   [ ... ]       # required, need not be normalised
//
// Energies come back multiplied by the unit; eMin/eMax are 0 when the file does
// not give them. Returns false with a filled-in error on any problem, so the
// caller can leave whatever spectrum it already had untouched.
//
// Shared by the sources that need a data table too large for a UI command
// argument (IBDGen, RockGammaGen, ...).
bool LoadSpectrumYaml(const std::string & path, std::vector<G4double> & energies,
                      std::vector<G4double> & flux, G4double & eMin, G4double & eMax,
                      std::string & error);
