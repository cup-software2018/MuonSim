#pragma once

#include <string>

#include "TObject.h"

class MCStep : public TObject {
public:
  MCStep();
  MCStep(const MCStep & step);
  virtual ~MCStep() = default;

  // Resets this step's own fields; TObject::Clear would not.
  void Clear(Option_t * opt = "") override;

  void SetStepLength(float val);
  void SetEnergyDeposit(float val);
  void SetEnergyDepositNonIonizing(float val);
  void SetKineticEnergy(float val);
  void SetGlobalTime(float val);
  void SetLocalTime(float val);
  void SetStepPoint(float x, float y, float z);
  void SetVolumeName(const char * name);
  void SetProcessName(const char * name);
  void SetDetectorID(int val);

  float GetStepLength() const;
  float GetEnergyDeposit() const;
  float GetEnergyDepositNonIonizing() const;
  float GetKineticEnergy() const;
  float GetGlobalTime() const;
  float GetLocalTime() const;
  void GetStepPoint(float & x, float & y, float & z) const;
  std::string GetVolumeName() const;
  std::string GetProcessName() const;
  int GetDetectorID() const;

protected:
  float fStepLength = 0;
  float fEnergyDeposit = 0;
  float fEnergyDepositNonIonizing = 0;
  float fKineticEnergy = 0;
  float fGlobalTime = 0;
  float fLocalTime = 0;
  float fX = 0, fY = 0, fZ = 0;
  std::string fVolumeName;
  std::string fProcessName;

  // Which readout element the step happened in, or -1 for none. Only the
  // geometry knows how to work this out -- a shared logical volume placed many
  // times has one name but many IDs -- so it is supplied by a callback and
  // stored here verbatim. See SteppingAction::DetectorIDTagger.
  int fDetectorID = -1;

  ClassDefOverride(MCStep, 4)
};

inline void MCStep::SetStepLength(float val) { fStepLength = val; }

inline void MCStep::SetEnergyDeposit(float val) { fEnergyDeposit = val; }

inline void MCStep::SetEnergyDepositNonIonizing(float val) { fEnergyDepositNonIonizing = val; }

inline void MCStep::SetKineticEnergy(float val) { fKineticEnergy = val; }

inline void MCStep::SetGlobalTime(float val) { fGlobalTime = val; }

inline void MCStep::SetLocalTime(float val) { fLocalTime = val; }

inline void MCStep::SetStepPoint(float x, float y, float z)
{
  fX = x;
  fY = y;
  fZ = z;
}

inline void MCStep::SetVolumeName(const char * val) { fVolumeName = val; }

inline void MCStep::SetProcessName(const char * val) { fProcessName = val; }

inline void MCStep::SetDetectorID(int val) { fDetectorID = val; }

inline float MCStep::GetStepLength() const { return fStepLength; }

inline float MCStep::GetEnergyDeposit() const { return fEnergyDeposit; }

inline float MCStep::GetEnergyDepositNonIonizing() const { return fEnergyDepositNonIonizing; }

inline float MCStep::GetKineticEnergy() const { return fKineticEnergy; }

inline float MCStep::GetGlobalTime() const { return fGlobalTime; }

inline float MCStep::GetLocalTime() const { return fLocalTime; }

inline void MCStep::GetStepPoint(float & x, float & y, float & z) const
{
  x = fX;
  y = fY;
  z = fZ;
}

inline std::string MCStep::GetVolumeName() const { return fVolumeName; }

inline std::string MCStep::GetProcessName() const { return fProcessName; }

inline int MCStep::GetDetectorID() const { return fDetectorID; }
