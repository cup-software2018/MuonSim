#pragma once

#include "G4VSensitiveDetector.hh"
#include "PMTHit.hh"

class G4Step;
class G4HCofThisEvent;

class PMTSD : public G4VSensitiveDetector {
public:
  PMTSD(G4String name);
  virtual ~PMTSD() = default;

  virtual void Initialize(G4HCofThisEvent *);
  virtual G4bool ProcessHits(G4Step *, G4TouchableHistory *) { return false; }
  virtual void EndOfEvent(G4HCofThisEvent *) {}
  virtual void clear() {}
  void DrawAll() {}
  void PrintAll() {}

  void SimpleHit(G4int ipmt, G4double time, G4double kineticEnergy, const G4ThreeVector & position,
                 const G4ThreeVector & momentum, const G4ThreeVector & polarization,
                 G4int iHitPhotonCount);

private:
  PMTHitsCollection * fPMTHitCollection;
};
