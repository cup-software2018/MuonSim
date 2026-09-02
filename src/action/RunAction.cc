#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4ios.hh"
#include "RootManager.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "Observer.hh"

RunAction::RunAction(RootManager * rootManager, std::vector<Observer *> observers)
  : fRootManager(rootManager),
    fObservers(std::move(observers))
{
}

void RunAction::BeginOfRunAction(const G4Run * run)
{
  G4cout << "### Run " << run->GetRunID() << " start." << G4endl;

  fRootManager->BeginOfRun(run);

  for (Observer * observer : fObservers) {
    if (!observer->IsEnabled()) continue;
    G4cout << "### observer on: " << observer->GetName() << G4endl;
    observer->BeginOfRun(run, fRootManager);
  }
}

void RunAction::EndOfRunAction(const G4Run * run)
{
  G4cout << "### Run " << run->GetRunID() << " end." << G4endl;

  // The source clock, printed for every run rather than by an observer, because the
  // live time is what a rate is divided by and pileup happens whether or not anyone
  // enabled anything. At the default 1 Bq the live time in seconds equals the decay
  // count and the piled-up figure stays at zero.
  if (auto * runManager = G4RunManager::GetRunManager()) {
    if (const auto * generator = dynamic_cast<const PrimaryGeneratorAction *>(
            runManager->GetUserPrimaryGeneratorAction())) {
      const G4double live = generator->GetUniversalTime() / CLHEP::second;
      const G4long planted = generator->GetPrimariesGenerated();
      const G4long piled = generator->GetPiledUp();
      G4cout << "### source: " << planted << " decays over " << live << " s live time"
             << " at " << generator->GetActivity() / CLHEP::becquerel << " Bq, window "
             << G4BestUnit(generator->GetEventWindow(), "Time") << G4endl;
      if (live > 0.)
        G4cout << "### divide counts by " << live << " s for a rate" << G4endl;
      if (piled > 0)
        G4cout << "### " << piled << " of those decays piled onto another one ("
               << 100. * piled / planted << "%)" << G4endl;
    }
  }

  // Before RootManager closes its file, which is where the observers' own trees
  // and objects live.
  for (Observer * observer : fObservers)
    if (observer->IsEnabled()) observer->EndOfRun(run, fRootManager);

  fRootManager->EndOfRun(run);
}
