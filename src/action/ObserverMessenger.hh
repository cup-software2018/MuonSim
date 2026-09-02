#pragma once

#include <string>
#include <vector>

#include "G4UImessenger.hh"

class G4UIcmdWithAString;
class G4UIcmdWithoutParameter;
class G4UIdirectory;
class Observer;

// /observer -- turns the registered observers on and off by name.
//
//   /observer/list                    every family
//   /observer/run/enable   <name>     or all
//   /observer/event/enable <name>
//   /observer/stack/enable <name>
//   /observer/track/enable <name>
//   /observer/step/enable  <name>
//   /observer/<family>/disable <name>
//
// Which observers a build HAS is settled in C++, where they register themselves;
// which ones a RUN uses belongs in the macro, next to the generator and the save
// options, because that is what an observer changes. One that kills tracks alters
// the physics, so it must be asked for and never arrive as a side effect of having
// been linked in.
//
// One messenger for every family: they differ in what they hook into, not in how
// they are addressed, and a single class keeps them from drifting apart. The lists
// are not owned here -- ActionInitialization keeps the observers alive.
//
// Families arrive as a NAMED list rather than one argument each. Five arguments of
// the same type, distinguished only by position, is a mistake waiting to be made --
// swap two and every observer still registers, under the wrong path.
class ObserverMessenger : public G4UImessenger {
public:
  struct FamilyInput {
    std::string name;
    std::vector<Observer *> observers;
  };

  explicit ObserverMessenger(std::vector<FamilyInput> families);
  ~ObserverMessenger() override;

  void SetNewValue(G4UIcommand * command, G4String newValue) override;

private:
  struct Family {
    std::string name;
    std::vector<Observer *> observers;
    G4UIdirectory * dir = nullptr;
    G4UIcmdWithAString * enableCmd = nullptr;
    G4UIcmdWithAString * disableCmd = nullptr;
  };

  void ListAll() const;
  void ListFamily(const Family & family) const;
  bool SetEnabled(Family & family, const G4String & name, bool on) const;

  G4UIdirectory * fDir = nullptr;
  G4UIcmdWithoutParameter * fListCmd = nullptr;
  std::vector<Family> fFamilies;
};
