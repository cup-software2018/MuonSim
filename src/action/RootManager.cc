#include <iomanip>
#include <iostream>

#include "G4EmSaturation.hh"
#include "G4HCofThisEvent.hh"
#include "G4LossTableManager.hh"
#include "G4OpticalPhoton.hh"
#include "G4ParticleDefinition.hh"
#include "G4PrimaryVertex.hh"
#include "G4SDManager.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIdirectory.hh"
#include "G4VPhysicalVolume.hh"
#include "MCPMT.hh"
#include "MCPhotonHit.hh"
#include "MCPrimary.hh"
#include "MCScint.hh"
#include "MCScintStep.hh"
#include "MCTrack.hh"
#include "PMTHit.hh"
#include "G4RunManager.hh"
#include "PrimaryGeneratorAction.hh"
#include "RootManager.hh"

using namespace std;
using namespace CLHEP;

RootManager::RootManager()
  : G4UImessenger(),
    fPMTHitCollId(-1),
    fPrimaryData(nullptr),
    fTrackData(nullptr),
    fPMTData(nullptr),
    fScintData(nullptr),
    fEventInfo(nullptr),
    fRootFile(nullptr)
{
  fTrackSaveFlag = kSaveNone;
  fStepSaveOption = 0;
  fHitPhotonSave = 0;
  fScintStepSave = 0;

  fROOTDir = new G4UIdirectory("/ROOT/");

  fTrackSaveOptCmd = new G4UIcmdWithAnInteger("/ROOT/savetrackopt", this);
  fStepSaveOptCmd = new G4UIcmdWithAnInteger("/ROOT/savestepopt", this);
  fHitPhotonSaveCmd = new G4UIcmdWithAnInteger("/ROOT/savehitphoton", this);
  fScintStepSaveCmd = new G4UIcmdWithAnInteger("/ROOT/savescintstep", this);

  G4cout << "RootManager::RootManager() created" << G4endl;
}

RootManager::~RootManager()
{
  delete fTrackSaveOptCmd;
  delete fStepSaveOptCmd;
  delete fHitPhotonSaveCmd;
  delete fScintStepSaveCmd;

  G4cout << "RootManager::RootManager() destroyed" << G4endl;
}

void RootManager::SetNewValue(G4UIcommand * command, G4String newValues)
{
  if (command == fTrackSaveOptCmd) fTrackSaveFlag = G4UIcmdWithAnInteger::GetNewIntValue(newValues);
  else if (command == fStepSaveOptCmd)
    fStepSaveOption = G4UIcmdWithAnInteger::GetNewIntValue(newValues);
  else if (command == fHitPhotonSaveCmd)
    fHitPhotonSave = G4UIcmdWithAnInteger::GetNewIntValue(newValues);
  else if (command == fScintStepSaveCmd)
    fScintStepSave = G4UIcmdWithAnInteger::GetNewIntValue(newValues);
}

void RootManager::BeginOfRun(const G4Run * aRun)
{
  OpenRootFile();
  Booking();

  G4int runId = aRun->GetRunID();
  G4int nEventToBeProcessed = aRun->GetNumberOfEventToBeProcessed();

  G4cout << G4endl;
  G4cout << "++++++++++++++++++ Run Initialized ++++++++++++++++++" << G4endl;
  G4cout << "   RunID              : " << runId << G4endl;
  G4cout << "   NEventToBeProcessed: " << nEventToBeProcessed << G4endl;
  G4cout << "++++++++++++++++++ Run Initialized ++++++++++++++++++" << G4endl;
  G4cout << G4endl;
}

void RootManager::EndOfRun(const G4Run * aRun)
{
  G4int runId = aRun->GetRunID();
  G4int nEventProcessed = aRun->GetNumberOfEvent();

  CloseRootFile();

  // Nulled, not just freed: between here and the next BeginOfRun these are
  // dangling otherwise, and a second /run/beamOn goes through BeginOfEvent first.
  delete fEventInfo;
  fEventInfo = nullptr;
  delete fPrimaryData;
  fPrimaryData = nullptr;
  delete fTrackData;
  fTrackData = nullptr;
  delete fPMTData;
  fPMTData = nullptr;
  delete fScintData;
  fScintData = nullptr;

  G4cout << G4endl;
  G4cout << "++++++++++++++++++  Run Finalized  ++++++++++++++++++" << G4endl;
  G4cout << "   RunID          : " << runId << G4endl;
  G4cout << "   NEventProcessed: " << nEventProcessed << G4endl;
  G4cout << "++++++++++++++++++  Run Finalized  ++++++++++++++++++" << G4endl;
  G4cout << G4endl;
}

void RootManager::BeginOfEvent(const G4Event *)
{
  fPrimaryData->Clear();
  fTrackData->Clear();
  fPMTData->Clear();
  fScintData->Clear();
  fScintAccum.clear();

  G4SDManager * SDman = G4SDManager::GetSDMpointer();
  if (fPMTHitCollId < 0) { fPMTHitCollId = SDman->GetCollectionID("pmtHitCollection"); }
}

void RootManager::EndOfEvent(const G4Event * anEvent)
{
  G4int eventId = anEvent->GetEventID() + 1; // event number starts from 1

  fEventInfo->SetEventNumber(eventId);

  // The source clock, so a rate can be worked out from the file alone: the last
  // event's value is the live time the run covers. Asked of the generator rather
  // than tracked here, because the generator is what advances it -- and it only
  // advances when a nucleus is planted, so the events of one split chain all carry
  // the time that chain started.
  if (auto * runManager = G4RunManager::GetRunManager()) {
    if (const auto * generator = dynamic_cast<const PrimaryGeneratorAction *>(
            runManager->GetUserPrimaryGeneratorAction()))
      fEventInfo->SetUniversalTime(generator->GetUniversalTime() / CLHEP::second);
  }

  // primary vertex
  G4int npvx = anEvent->GetNumberOfPrimaryVertex();
  for (int i = 0; i < npvx; i++) {
    G4PrimaryVertex * pvx = anEvent->GetPrimaryVertex(i);
    double x0 = pvx->GetX0();
    double y0 = pvx->GetY0();
    double z0 = pvx->GetZ0();
    double t0 = pvx->GetT0();

    int nptl = pvx->GetNumberOfParticle();
    for (int j = 0; j < nptl; j++) {
      G4PrimaryParticle * ptl = pvx->GetPrimary(j);

      MCPrimary * prim = fPrimaryData->Add();
      prim->SetT0(t0);
      prim->SetVertex(x0, y0, z0);
      prim->SetMomentum(ptl->GetPx(), ptl->GetPy(), ptl->GetPz());
      prim->SetKineticEnergy(ptl->GetKineticEnergy());
      prim->SetTrackId(ptl->GetTrackID());
      prim->SetPDGCode(ptl->GetG4code()->GetPDGEncoding());
    }
  }

  // PMT Hits
  G4HCofThisEvent * hits = anEvent->GetHCofThisEvent();
  PMTHitsCollection * pmtHC = nullptr;

  if (hits && (fPMTHitCollId >= 0)) { pmtHC = (PMTHitsCollection *)(hits->GetHC(fPMTHitCollId)); }

  if (pmtHC) {
    G4int nPMT = pmtHC->entries();

    for (int i = 0; i < nPMT; i++) {
      const PMTHit * pmt = (*pmtHC)[i];
      int pmtId = pmt->GetPMTId();

      MCPMT * mcpmt = fPMTData->Add();
      mcpmt->SetId(pmtId);

      int nph = pmt->GetNHit();
      if (fHitPhotonSave) {
        for (int j = 0; j < nph; j++) {
          MCPhotonHit * ph = pmt->GetHit(j);
          mcpmt->AddHit(ph);
        }
        mcpmt->Sort();
      }
    }
    fPMTData->Sort();
  }

  FlushScint();

  fEventTree->Fill();
}

void RootManager::RecordTrack(const G4Track * gtrack)
{
  if (!fTrackSaveFlag) return;

  const G4ParticleDefinition * pdef = gtrack->GetParticleDefinition();
  const G4String & pname = pdef->GetParticleName();
  const G4VProcess * proc = gtrack->GetCreatorProcess();
  const G4String procName = proc ? proc->GetProcessName() : "";

  if (pname == "opticalphoton") {
    if (!(fTrackSaveFlag & kSaveOptical)) return;
  }
  // Gammas get their own bit. They used to fall through to kSaveHeavy, which put
  // them in with the muons and hadrons -- half of every output file, and no way to
  // ask for one without the other. Underground work needs them selectable on their
  // own: they are the background channel that matters for dark matter and
  // double-beta searches.
  else if (pname == "gamma") {
    if (!(fTrackSaveFlag & kSaveGamma)) return;
  }
  else if (pname == "e-" || pname == "e+") {
    bool isEMShower = procName == "compt" || procName == "phot" || procName == "conv" ||
                      procName == "annihil" || G4StrUtil::contains(procName, "Ioni") ||
                      G4StrUtil::contains(procName, "Brem");
    if (!(fTrackSaveFlag & (isEMShower ? kSaveEMShower : kSaveElec))) return;
  }
  else {
    if (!(fTrackSaveFlag & kSaveHeavy)) return;
  }

  MCTrack * mtrack = fTrackData->FindTrack(gtrack->GetTrackID());
  if (!mtrack) {
    mtrack = fTrackData->Add();
    mtrack->SetPDGCode(pdef->GetPDGEncoding());
    mtrack->SetTrackId(gtrack->GetTrackID());
    mtrack->SetParentId(gtrack->GetParentID());
    mtrack->SetVertex(gtrack->GetVertexPosition().x(), gtrack->GetVertexPosition().y(),
                      gtrack->GetVertexPosition().z());
    mtrack->SetMomentumDir(gtrack->GetVertexMomentumDirection().x(),
                           gtrack->GetVertexMomentumDirection().y(),
                           gtrack->GetVertexMomentumDirection().z());
    mtrack->SetKineticEnergy(gtrack->GetVertexKineticEnergy());
    mtrack->SetGlobalTime(gtrack->GetGlobalTime());
    mtrack->SetLocalTime(gtrack->GetLocalTime());
    if (proc) mtrack->SetProcessName(procName.data());
  }
}

void RootManager::RecordStep(const G4Step * aStep, const G4VProcess * proc, G4int detectorID)
{
  // Deliberately ahead of every early return below: a scintillator's
  // energy must not depend on which track types the run happens to be saving.
  RecordScintDeposit(aStep, detectorID);

  if (!proc || !fTrackSaveFlag || !fStepSaveOption) return;

  G4Track * track = aStep->GetTrack();
  const G4String & pname = track->GetParticleDefinition()->GetParticleName();

  // Classify this track into a StepSaveFlag bit.
  G4int stepBit = kStepNone;
  if (pname == "opticalphoton") { stepBit = kStepOptical; }
  else if (pname == "gamma") {
    stepBit = kStepGamma;
  }
  else if (pname == "e-" || pname == "e+") {
    const G4VProcess * creator = track->GetCreatorProcess();
    const G4String creatorName = creator ? creator->GetProcessName() : "";
    bool isEMShower = creatorName == "compt" || creatorName == "phot" || creatorName == "conv" ||
                      creatorName == "annihil" || G4StrUtil::contains(creatorName, "Ioni") ||
                      G4StrUtil::contains(creatorName, "Brem");
    stepBit = isEMShower ? kStepEMShower : kStepElec;
  }
  else {
    stepBit = kStepHeavy;
  }

  if (!(fStepSaveOption & stepBit)) return;

  G4StepPoint * postStepPoint = aStep->GetPostStepPoint();
  G4VPhysicalVolume * volume = postStepPoint->GetPhysicalVolume();
  if (!volume) return;

  MCTrack * mcTrack = fTrackData->FindTrack(track->GetTrackID());
  if (!mcTrack) return;

  MCStep * mcStep = mcTrack->AddStep();
  mcStep->SetStepLength(aStep->GetStepLength());
  mcStep->SetEnergyDeposit(aStep->GetTotalEnergyDeposit());
  mcStep->SetEnergyDepositNonIonizing(aStep->GetNonIonizingEnergyDeposit());
  mcStep->SetKineticEnergy(postStepPoint->GetKineticEnergy());
  mcStep->SetGlobalTime(postStepPoint->GetGlobalTime());
  mcStep->SetLocalTime(postStepPoint->GetLocalTime());
  G4ThreeVector pos = postStepPoint->GetPosition();
  mcStep->SetStepPoint(pos.x(), pos.y(), pos.z());
  mcStep->SetVolumeName(volume->GetName().data());
  mcStep->SetDetectorID(detectorID);
  mcStep->SetProcessName(proc->GetProcessName().data());
}

void RootManager::OpenRootFile()
{
  fRootFile = new TFile(fRootFilename.c_str(), "recreate");
  G4cout << "RootManager::OpenRootFile(): output file " << fRootFilename << " opened ..." << G4endl;
}

TTree * RootManager::AddTree(const char * name, const char * title)
{
  if (fRootFile == nullptr) {
    G4Exception("RootManager::AddTree", "ROOT001", JustWarning,
                "no output file open yet -- ask for the tree from BeginOfRun");
    return nullptr;
  }
  // In OUR file, whatever directory happens to be current elsewhere.
  fRootFile->cd();
  return new TTree(name, title);
}

void RootManager::Add(TObject * object)
{
  if (object) fExtras.push_back(object);
}

void RootManager::CloseRootFile()
{
  fRootFile->cd();

  // Written here rather than by whoever made them, so that everything reaches the
  // file exactly once: a Write() from the owner and this one would leave two cycles
  // of the same object.
  for (TObject * extra : fExtras)
    extra->Write();

  fRootFile->Write();
  fRootFile->Close();

  // The file owned the trees and deleted them; these it did not.
  for (TObject * extra : fExtras)
    delete extra;
  fExtras.clear();

  delete fRootFile;
  fRootFile = NULL;
}

void RootManager::RecordScintDeposit(const G4Step * aStep, G4int detectorID)
{
  if (detectorID < 0) return;

  const G4double edep = aStep->GetTotalEnergyDeposit();
  if (edep <= 0.) return;

  G4Track * track = aStep->GetTrack();
  const G4ParticleDefinition * pdef = track->GetParticleDefinition();
  if (pdef == G4OpticalPhoton::Definition()) return;

  // Deferred to the first deposit: EM physics fills the saturation tables during
  // initialisation, after the geometry that named these volumes was built.
  if (!fEmSaturation) fEmSaturation = G4LossTableManager::Instance()->EmSaturation();
  const G4double edepVisible =
      fEmSaturation ? fEmSaturation->VisibleEnergyDepositionAtAStep(aStep) : edep;

  // Read off the pre-step point: that is where the track was while it was losing
  // the energy. MCStep's own rows use the post-step point instead, because they
  // describe where a step ended rather than where it deposited.
  const G4StepPoint * pre = aStep->GetPreStepPoint();
  const G4double time = pre->GetGlobalTime();
  const G4ThreeVector & pos = pre->GetPosition();

  auto it = fScintAccum.find(detectorID);
  if (it == fScintAccum.end()) {
    it = fScintAccum.emplace(detectorID, ScintAccum()).first;
    it->second.time = time;
    it->second.timeLast = time;
    it->second.position = pos;
  }
  ScintAccum & sc = it->second;

  sc.edep += edep;
  sc.edepVisible += edepVisible;
  sc.tracks.insert(track->GetTrackID());
  sc.edepByPdg[pdef->GetPDGEncoding()] += edep;

  // Earliest deposit by TIME, not by arrival order: Geant4 hands tracks over one
  // at a time, and a track taken later can well have deposited earlier -- several
  // primaries in one event, or a track resumed after being suspended.
  if (time < sc.time) {
    sc.time = time;
    sc.position = pos;
  }
  if (time > sc.timeLast) sc.timeLast = time;

  if (fScintStepSave) {
    sc.steps.emplace_back();
    MCScintStep & rec = sc.steps.back();
    rec.SetPdgCode(pdef->GetPDGEncoding());
    rec.SetTrackId(track->GetTrackID());
    rec.SetEnergyDeposit(edep);
    rec.SetEnergyDepositVisible(edepVisible);
    rec.SetEnergyDepositNonIonizing(aStep->GetNonIonizingEnergyDeposit());
    rec.SetKineticEnergy(pre->GetKineticEnergy());
    rec.SetGlobalTime(time);
    rec.SetLocalTime(pre->GetLocalTime());
    rec.SetStepLength(aStep->GetStepLength());
    rec.SetStepPoint(pos.x(), pos.y(), pos.z());
    if (pre->GetPhysicalVolume()) rec.SetVolumeName(pre->GetPhysicalVolume()->GetName().data());
    const G4VProcess * defining = aStep->GetPostStepPoint()->GetProcessDefinedStep();
    rec.SetProcessName(defining ? defining->GetProcessName().data() : "");
    rec.SetDetectorID(detectorID);
  }
}

void RootManager::FlushScint()
{
  for (const auto & kv : fScintAccum) {
    const ScintAccum & sc = kv.second;

    MCScint * out = fScintData->Add();
    out->SetId(kv.first);
    out->SetEnergyDeposit(sc.edep);
    out->SetEnergyDepositVisible(sc.edepVisible);
    out->SetTime(sc.time);
    out->SetTimeLast(sc.timeLast);
    out->SetPosition(sc.position.x(), sc.position.y(), sc.position.z());
    out->SetNTrack((int)sc.tracks.size());

    G4int dominant = 0;
    G4double most = 0.;
    for (const auto & pe : sc.edepByPdg) {
      if (pe.second > most) {
        most = pe.second;
        dominant = pe.first;
      }
    }
    out->SetPdgDominant(dominant);

    for (const auto & rec : sc.steps)
      *out->AddStep() = rec;
  }
  fScintData->Sort();
}

void RootManager::AddRegion(G4int trackId, G4int regionBit)
{
  MCTrack * track = fTrackData->FindTrack(trackId);
  if (track) track->AddRegion(regionBit);
}

void RootManager::Booking()
{
  // All five, so ownership is one rule rather than four plus an exception.
  // fScintData used to be left null here and handed to Branch that way: ROOT then
  // made one of its own, which worked, but EndOfRun deleted a pointer this class
  // never allocated. We allocate, we delete.
  fEventInfo = new MCEventInfo();
  fPrimaryData = new MCPrimaryData();
  fTrackData = new MCTrackData();
  fPMTData = new MCPMTData();
  fScintData = new MCScintData();

  fEventTree = new TTree("Event", "Event");
  fEventTree->Branch("MCEventInfo", &fEventInfo);
  fEventTree->Branch("MCPrimaryData", &fPrimaryData);
  fEventTree->Branch("MCTrackData", &fTrackData);
  fEventTree->Branch("MCPMTData", &fPMTData);
  fEventTree->Branch("MCScintData", &fScintData);
}
