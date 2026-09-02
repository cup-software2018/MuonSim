#include <cmath>

#include "AbsVertexGen.hh"
#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4Exception.hh"
#include "G4StackManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"
#include "Randomize.hh"
#include "PrimaryGeneratorAction.hh"
#include "PrimaryGeneratorMessenger.hh"


PrimaryGeneratorAction::PrimaryGeneratorAction()
  : fActivity(1. * becquerel),
    fEventWindow(1. * microsecond)
{
  // Geant4 defines "Bq" itself but not its multiples, so each one is checked on its
  // own. Guarding the whole block on "Bq" being absent -- which is what this did
  // first -- skips every multiple and leaves "/gen/activity 1 MBq" aborting the run
  // with "The unit 'MBq' does not exist in the Units Table!".
  struct ActivityUnit {
    const char * name;
    const char * symbol;
    G4double value;
  };
  const ActivityUnit units[] = {
      {"becquerel", "Bq", becquerel},
      {"kilobecquerel", "kBq", CLHEP::kilobecquerel},
      {"megabecquerel", "MBq", CLHEP::megabecquerel},
      {"gigabecquerel", "GBq", CLHEP::gigabecquerel},
      {"curie", "Ci", curie},
  };
  for (const auto & unit : units)
    if (!G4UnitDefinition::IsUnitDefined(unit.symbol))
      new G4UnitDefinition(unit.name, unit.symbol, "Activity", unit.value);

  fMessenger = std::make_unique<PrimaryGeneratorMessenger>(this);
}


PrimaryGeneratorAction::~PrimaryGeneratorAction() = default;


void PrimaryGeneratorAction::AddVertex(std::unique_ptr<AbsVertexGen> vertex)
{
  if (vertex) fVertices.push_back(std::move(vertex));
}


void PrimaryGeneratorAction::ClearVertices() { fVertices.clear(); }


std::size_t PrimaryGeneratorAction::GetNumberOfVertices() const { return fVertices.size(); }


void PrimaryGeneratorAction::ListVertices() const
{
  if (fVertices.empty()) {
    G4cout << "/gen/list: no vertices defined -- /run/beamOn would fail" << G4endl;
    return;
  }
  G4cout << "/gen/list: " << fVertices.size() << " vertex(es), all fired every event:" << G4endl;
  for (std::size_t i = 0; i < fVertices.size(); i++)
    G4cout << "  [" << i << "] " << fVertices[i]->GetDescription() << G4endl;
}


void PrimaryGeneratorAction::GeneratePrimaries(G4Event * anEvent)
{
  if (fVertices.empty()) {
    G4Exception("PrimaryGeneratorAction::GeneratePrimaries", "GEN001", FatalException,
                "no primary vertex defined -- add one with /gen/vertex before /run/beamOn "
                "(cosmic muons are /gen/vertex cosmic input <flux.root>)");
    return;
  }
  // Geant4 has not moved the deferred stack into the urgent one yet -- that happens
  // in ProcessOneEvent, after this -- so what it holds here is exactly what the last
  // event handed on. An event that has inherited tracks is already occupied.
  if (fYieldToDeferred) {
    auto * eventManager = G4EventManager::GetEventManager();
    auto * stackManager = eventManager ? eventManager->GetStackManager() : nullptr;
    if (stackManager && stackManager->GetNPostponedTrack() > 0) return;
  }

  // The decay that opens the event. Either one drawn last time and found to be out
  // of reach, or a fresh gap from where the clock stands.
  const G4double t0 = (fNextDecayTime >= 0.) ? fNextDecayTime : fUniversalTime + NextGap();
  fNextDecayTime = -1.;
  fUniversalTime = t0;
  PlantOne(anEvent, 0.);

  // Then everything else that falls inside the window, each at its real offset from
  // the opening decay. At 1 Bq the mean gap is a second against a microsecond
  // window, so this loop never runs once; at a real activity it runs as often as the
  // Poisson statistics say it should. That is the whole mechanism -- there is no
  // pileup switch, only an activity.
  while (true) {
    const G4double next = fUniversalTime + NextGap();
    if (next - t0 > fEventWindow) {
      fNextDecayTime = next;   // it opens the NEXT event; do not redraw it
      break;
    }
    fUniversalTime = next;
    PlantOne(anEvent, next - t0);
    fPiledUp++;
  }
}

G4double PrimaryGeneratorAction::NextGap()
{
  if (fActivity <= 0.) return 0.;
  return -std::log(G4UniformRand()) / fActivity;
}

void PrimaryGeneratorAction::PlantOne(G4Event * anEvent, G4double timeInEvent)
{
  for (const auto & vertex : fVertices) {
    vertex->SetTime(timeInEvent);
    vertex->GenerateVertex(anEvent);
  }
  fPrimariesGenerated++;
}
