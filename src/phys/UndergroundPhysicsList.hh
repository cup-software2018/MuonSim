#pragma once

#include "Shielding.hh"

class UndergroundPhysicsList : public Shielding {
public:
  UndergroundPhysicsList();
  ~UndergroundPhysicsList() override;

  void SetCuts() override;
};
