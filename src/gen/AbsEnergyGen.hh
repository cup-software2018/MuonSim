#pragma once

#include <string>

#include "G4Types.hh"

class AbsEnergyGen {
public:
  AbsEnergyGen() = default;
  virtual ~AbsEnergyGen() = default;

  virtual const std::string & GetKey() const = 0;
  virtual G4double GenerateEnergy() = 0;
};
