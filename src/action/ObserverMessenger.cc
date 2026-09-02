#include <utility>

#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIdirectory.hh"
#include "G4ios.hh"

#include "Observer.hh"
#include "ObserverMessenger.hh"

ObserverMessenger::ObserverMessenger(std::vector<FamilyInput> families)
{
  fDir = new G4UIdirectory("/observer/");
  fDir->SetGuidance("Observers registered by this build. All start off.");

  fListCmd = new G4UIcmdWithoutParameter("/observer/list", this);
  fListCmd->SetGuidance("List every observer of every family and whether it is on.");
  fListCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  // Whatever order the caller gave, which is the order /observer/list prints.
  for (auto & input : families)
    fFamilies.push_back({std::move(input.name), std::move(input.observers), nullptr, nullptr,
                         nullptr});

  for (Family & family : fFamilies) {
    const std::string path = "/observer/" + family.name + "/";
    family.dir = new G4UIdirectory(path.c_str());
    family.dir->SetGuidance(("Observers hooked into " + family.name + ".").c_str());

    family.enableCmd = new G4UIcmdWithAString((path + "enable").c_str(), this);
    family.enableCmd->SetGuidance("Turn an observer on for this run: <name> or all.");
    family.enableCmd->SetGuidance("Ask before /run/beamOn -- observers are given their");
    family.enableCmd->SetGuidance("output at the start of the run.");
    family.enableCmd->SetParameterName("name", false);
    family.enableCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    family.disableCmd = new G4UIcmdWithAString((path + "disable").c_str(), this);
    family.disableCmd->SetGuidance("Turn an observer off again: <name> or all.");
    family.disableCmd->SetParameterName("name", false);
    family.disableCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
  }
}

ObserverMessenger::~ObserverMessenger()
{
  for (Family & family : fFamilies) {
    delete family.disableCmd;
    delete family.enableCmd;
    delete family.dir;
  }
  delete fListCmd;
  delete fDir;
}

void ObserverMessenger::ListFamily(const Family & family) const
{
  if (family.observers.empty()) {
    G4cout << "  " << family.name << ": none registered" << G4endl;
    return;
  }
  G4cout << "  " << family.name << ":" << G4endl;
  for (const Observer * observer : family.observers) {
    if (!observer) continue;
    G4cout << "    " << (observer->IsEnabled() ? "[on ] " : "[off] ") << observer->GetName();
    const std::string what = observer->Describe();
    if (!what.empty()) G4cout << "  -- " << what;
    G4cout << G4endl;
  }
}

void ObserverMessenger::ListAll() const
{
  G4cout << "/observer/list:" << G4endl;
  for (const Family & family : fFamilies) ListFamily(family);
}

bool ObserverMessenger::SetEnabled(Family & family, const G4String & name, bool on) const
{
  if (name == "all") {
    for (Observer * observer : family.observers)
      if (observer) observer->SetEnabled(on);
    return !family.observers.empty();
  }

  for (Observer * observer : family.observers)
    if (observer && observer->GetName() == name) {
      observer->SetEnabled(on);
      return true;
    }
  return false;
}

void ObserverMessenger::SetNewValue(G4UIcommand * command, G4String newValue)
{
  if (command == fListCmd) {
    ListAll();
    return;
  }

  for (Family & family : fFamilies) {
    const bool on = (command == family.enableCmd);
    if (!on && command != family.disableCmd) continue;

    if (SetEnabled(family, newValue, on)) {
      G4cout << "/observer/" << family.name << "/" << (on ? "enable" : "disable") << ": "
             << newValue << G4endl;
      return;
    }

    // Named something that is not there: say so rather than leaving the run to
    // behave as though the observer had been asked for.
    G4cerr << "/observer/" << family.name << "/" << (on ? "enable" : "disable")
           << ": no observer named '" << newValue << "'" << G4endl;
    ListFamily(family);
    return;
  }
}
