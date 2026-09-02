#pragma once

#include <vector>

#include "TObject.h"

#include "MCScintStep.hh"

// One scintillator's response in one event.
//
// This is an aggregate, not a hit list: every energy deposit the scintillator saw
// during the event is summed into it. That is the whole point of reading a
// scintillator through a sensitive detector rather than through the step record
// -- the step record only holds steps of tracks the run chose to save, so the
// energy carried off by delta electrons goes missing unless the output is blown
// up to keep every electron step.
//
// What one entry stands for is the DETECTOR's business, not this class's: an entry
// is created per distinct id, and the ids come from the tagger the geometry hands
// to SteppingAction. Whether that id counts a bar, a plate, a crystal or a readout
// end is decided there. This class only stores it, so it carries no assumption
// about segmentation and needs no change when the geometry does.
class MCScint : public TObject {
public:
  MCScint();
  MCScint(const MCScint & scint);
  virtual ~MCScint() = default;

  void Clear(Option_t * opt = "") override;

  void SetId(int id);
  void SetEnergyDeposit(float val);
  void SetEnergyDepositVisible(float val);
  void SetTime(float val);
  void SetTimeLast(float val);
  void SetPosition(float x, float y, float z);
  void SetNTrack(int val);
  void SetPdgDominant(int val);

  // The individual deposits, when the run asked for them (/ROOT/savescintstep).
  MCScintStep * AddStep();
  int GetNStep() const;
  MCScintStep * GetStep(int i) const;

  int GetId() const;
  float GetEnergyDeposit() const;
  float GetEnergyDepositVisible() const;
  float GetTime() const;
  float GetTimeLast() const;
  void GetPosition(float & x, float & y, float & z) const;
  int GetNTrack() const;
  int GetPdgDominant() const;

  bool IsSortable() const override { return true; }
  int Compare(const TObject * object) const override;

  void Print(Option_t * opt = "") const override;

private:
  int fId = -1;

  float fEnergyDeposit = 0;        // sum over the event [MeV]
  float fEnergyDepositVisible = 0; // the same, Birks-quenched [MeV]

  // Times of the first and last deposit. For a single crossing these differ by
  // the flight time across the volume, so in a thin scintillator they are not a
  // resolvable entry/exit pair. A large gap means late activity in the same one: a
  // stopped muon decaying (tau = 2.2 us) is the case to look for.
  float fTime = 0;
  float fTimeLast = 0;

  // Where the first deposit happened, i.e. on the entry face for a through-going
  // particle. Together with the neighbouring scintillators that fired it gives the
  // incidence direction.
  float fX = 0, fY = 0, fZ = 0;

  int fNTrack = 0; // distinct tracks that deposited here

  // PDG code of whichever species deposited the most energy here. An aggregate,
  // so it is lossy on purpose: over half of all hit scintillators see two species
  // at once, and for a through-going muon only about 60% of the energy is the
  // muon's own -- the rest is its delta electrons. Turn the step list on to see
  // the whole picture; this is what survives with it off.
  int fPdgDominant = 0;

  // One entry per energy deposit. Empty unless the run asked for them: the
  // aggregate above is enough for most work, this is the detail behind it.
  std::vector<MCScintStep> fSteps;

  ClassDefOverride(MCScint, 3)
};

inline void MCScint::SetId(int id) { fId = id; }
inline void MCScint::SetEnergyDeposit(float val) { fEnergyDeposit = val; }
inline void MCScint::SetEnergyDepositVisible(float val) { fEnergyDepositVisible = val; }
inline void MCScint::SetTime(float val) { fTime = val; }
inline void MCScint::SetTimeLast(float val) { fTimeLast = val; }
inline void MCScint::SetNTrack(int val) { fNTrack = val; }
inline void MCScint::SetPdgDominant(int val) { fPdgDominant = val; }

inline void MCScint::SetPosition(float x, float y, float z)
{
  fX = x;
  fY = y;
  fZ = z;
}

inline int MCScint::GetId() const { return fId; }
inline float MCScint::GetEnergyDeposit() const { return fEnergyDeposit; }
inline float MCScint::GetEnergyDepositVisible() const { return fEnergyDepositVisible; }
inline float MCScint::GetTime() const { return fTime; }
inline float MCScint::GetTimeLast() const { return fTimeLast; }
inline int MCScint::GetNTrack() const { return fNTrack; }
inline int MCScint::GetPdgDominant() const { return fPdgDominant; }

inline int MCScint::GetNStep() const { return (int)fSteps.size(); }

inline MCScintStep * MCScint::GetStep(int n) const
{
  return const_cast<MCScintStep *>(&fSteps[n]);
}

inline void MCScint::GetPosition(float & x, float & y, float & z) const
{
  x = fX;
  y = fY;
  z = fZ;
}
