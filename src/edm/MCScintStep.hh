#pragma once

#include "MCStep.hh"

// One energy deposit inside a scintillator.
//
// It is an MCStep, because that is what it is: deposit, non-ionizing deposit,
// step length, kinetic energy, times, position, volume, the process that ended
// the step, the detector ID -- all of it means the same thing here. Only three
// things have to be added, and each for a reason:
//
//   fPdgCode, fTrackId    An MCStep inside an MCTrack inherits its identity from
//                         the track. These hang off an MCScint instead, so they
//                         carry their own.
//   fEnergyDepositVisible The Birks-quenched deposit -- the number
//                         G4Scintillation multiplies by the light yield, so it is
//                         the energy that actually drives the photon count. NOT
//                         the same as the non-ionizing deposit.
//
// They live here rather than in MCStep because MCStep is paid for on every step
// of every saved track, of order 10^5 a run against a few hundred scintillator
// deposits, and because quenching is a scintillator's business, not every
// volume's.
//
// NOTE for analysis macros: interpreted ROOT macros should include the whole
// related set of headers (MCScintData.hh together with MCScint.hh, MCTrackData.hh
// with MCTrack.hh and MCStep.hh). Including one of a pair on its own leaves cling
// resolving an incomplete type out of the dictionary payload.
class MCScintStep : public MCStep {
public:
  MCScintStep();
  MCScintStep(const MCScintStep & step);
  virtual ~MCScintStep() = default;

  void Clear(Option_t * opt = "") override;

  void SetPdgCode(int val);
  void SetTrackId(int val);
  void SetEnergyDepositVisible(float val);

  int GetPdgCode() const;
  int GetTrackId() const;
  float GetEnergyDepositVisible() const;

  void Print(Option_t * opt = "") const override;

private:
  int fPdgCode = 0;
  int fTrackId = 0;
  float fEnergyDepositVisible = 0;

  ClassDefOverride(MCScintStep, 1)
};

inline void MCScintStep::SetPdgCode(int val) { fPdgCode = val; }
inline void MCScintStep::SetTrackId(int val) { fTrackId = val; }
inline void MCScintStep::SetEnergyDepositVisible(float val) { fEnergyDepositVisible = val; }

inline int MCScintStep::GetPdgCode() const { return fPdgCode; }
inline int MCScintStep::GetTrackId() const { return fTrackId; }
inline float MCScintStep::GetEnergyDepositVisible() const { return fEnergyDepositVisible; }
