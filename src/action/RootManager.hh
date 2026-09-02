#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "TFile.h"
#include "TTree.h"

#include "G4Event.hh"
#include "G4Run.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4UImessenger.hh"
#include "G4VProcess.hh"
#include "MCEventInfo.hh"
#include "MCPMTData.hh"
#include "MCPrimaryData.hh"
#include "G4ThreeVector.hh"
#include "MCScintData.hh"
#include "MCScintStep.hh"
#include "MCTrackData.hh"

class G4EmSaturation;
class G4UIdirectory;
class G4UIcmdWithAnInteger;

class RootManager : public G4UImessenger {
public:
  // Bit flags for /ROOT/savetrackopt — combine with bitwise OR.
  enum TrackSaveFlag : G4int {
    kSaveNone     = 0,
    kSaveHeavy    = 1 << 0,  // muons, hadrons, ions -- and nothing else
    kSaveElec     = 1 << 1,  // e± not produced by EM-shower processes
    kSaveGamma    = 1 << 2,  // gammas
    kSaveEMShower = 1 << 3,  // e± from compt/phot/conv/annihil/Ioni/Brem
    kSaveOptical  = 1 << 4,  // optical photons
  };

  // Bit flags for /ROOT/savestepopt — same bit positions as TrackSaveFlag.
  enum StepSaveFlag : G4int {
    kStepNone     = 0,
    kStepHeavy    = 1 << 0,  // record steps for heavy tracks (muon/hadron/ion)
    kStepElec     = 1 << 1,  // record steps for non-shower e±
    kStepGamma    = 1 << 2,  // record steps for gammas
    kStepEMShower = 1 << 3,  // record steps for EM-shower e±
    kStepOptical  = 1 << 4,  // record steps for optical photons
  };

  RootManager();
  virtual ~RootManager();

  virtual void SetNewValue(G4UIcommand *, G4String);

  virtual void BeginOfRun(const G4Run *);
  virtual void EndOfRun(const G4Run *);
  virtual void BeginOfEvent(const G4Event *);
  virtual void EndOfEvent(const G4Event *);
  virtual void RecordTrack(const G4Track *);
  // detectorID is stored verbatim in MCStep; -1 means "no readout element".
  virtual void RecordStep(const G4Step *, const G4VProcess *, G4int detectorID = -1);
  void AddRegion(G4int trackId, G4int regionBit);

  void SetRootFilename(const char * fname) { fRootFilename = fname; }
  void OpenRootFile();
  void CloseRootFile();

  void Booking();

  // ---- output belonging to somebody else -----------------------------------
  //
  // A SteppingObserver, or anything else with its own thing to record, asks for its
  // tree here rather than opening a file of its own. One file per run then holds
  // everything the run produced, and nothing has to guess which ROOT directory
  // happens to be current -- getting that wrong attaches a tree to the wrong file,
  // which is silent until the file turns out to be empty.
  //
  // Call it from BeginOfRun, after this class has opened the file: RunAction calls
  // the observers in that order. The tree belongs to the file, so it is gone when
  // the run ends and has to be asked for again next run.
  TTree * AddTree(const char * name, const char * title = "");

  // Anything else to be written when the run ends -- a histogram, a TParameter of
  // metadata. Ownership passes here: it is written and deleted with the file.
  void Add(TObject * object);

private:
  // One scintillator building up over an event, keyed by the detector ID the
  // geometry's tagger gave the step. What an ID stands for is the detector's
  // business -- see MCScint.
  //
  // This lives here, not in a sensitive detector, because the step is already in
  // hand: SteppingAction hands every step to RecordStep along with the ID the
  // geometry says it is in, so there is nothing an SD would add. Sums are kept in
  // double and written out once, at the end of the event.
  struct ScintAccum {
    G4double edep = 0.;
    G4double edepVisible = 0.;
    G4double time = 0.;
    G4double timeLast = 0.;
    G4ThreeVector position; // where the earliest deposit was
    std::set<G4int> tracks;
    std::map<G4int, G4double> edepByPdg;
    std::vector<MCScintStep> steps;
  };

  // Folds one deposit into its scintillator. Called for EVERY step, before the
  // save-option gates: those decide what step rows reach the file, and the
  // scintillator sums must not depend on them. Keying off them would lose the
  // energy carried by tracks the run chose not to save -- about a fifth of it,
  // measured -- and recovering that by saving every electron step costs eight
  // times the rows.
  void RecordScintDeposit(const G4Step *, G4int detectorID);

  // Turns the accumulated sums into MCScintData. Called at end of event.
  void FlushScint();

  G4UIdirectory * fROOTDir;

  G4UIcmdWithAnInteger * fTrackSaveOptCmd;
  G4UIcmdWithAnInteger * fStepSaveOptCmd;
  G4UIcmdWithAnInteger * fHitPhotonSaveCmd;
  G4UIcmdWithAnInteger * fScintStepSaveCmd;

  G4int fTrackSaveFlag;
  G4int fStepSaveOption;
  G4int fHitPhotonSave;
  G4int fScintStepSave;

  G4int fPMTHitCollId;

  // Not trees: those the file owns. These are written and deleted in CloseRootFile.
  std::vector<TObject *> fExtras;
  std::map<G4int, ScintAccum> fScintAccum;
  G4EmSaturation * fEmSaturation = nullptr;

  MCPrimaryData * fPrimaryData;
  MCTrackData * fTrackData;
  MCPMTData * fPMTData;
  MCScintData * fScintData;
  MCEventInfo * fEventInfo;

  std::string fRootFilename;
  TFile * fRootFile;
  TTree * fRunTree;
  TTree * fEventTree;
};
