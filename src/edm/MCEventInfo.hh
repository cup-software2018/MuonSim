#pragma once

#include "TObject.h"

class MCEventInfo : public TObject {
public:
  MCEventInfo();
  MCEventInfo(const MCEventInfo & info);
  virtual ~MCEventInfo();

  void SetEventNumber(unsigned int n);
  unsigned int GetEventNumber() const;

  // When this event happened on the source's own clock, in SECONDS.
  //
  // Not a detail of the event, but the thing rates are divided by: the run's last
  // value is the live time it covers, so a count rate is counts / that. It is set
  // from /gen/activity, which schedules each source decay a Poisson gap after the
  // last one. At the default of 1 Bq the gaps average a second, so the live time in
  // seconds equals the number of decays and dividing by it is exactly the per-decay
  // normalisation -- the old arithmetic as a special case of the new.
  //
  // A chain split across several events carries the time of the decay that STARTED
  // it, the same value on each, because the clock advances only when a new nucleus
  // is planted. Within-chain delays were rebased away by the event window.
  void SetUniversalTime(double seconds);
  double GetUniversalTime() const;

  void Print(Option_t * opt = "") const override;

private:
  unsigned int fEventNumber = 0;
  double fUniversalTime = 0.;

  ClassDefOverride(MCEventInfo, 2)
};

inline void MCEventInfo::SetEventNumber(unsigned int n) { fEventNumber = n; }
inline unsigned int MCEventInfo::GetEventNumber() const { return fEventNumber; }

inline void MCEventInfo::SetUniversalTime(double seconds) { fUniversalTime = seconds; }
inline double MCEventInfo::GetUniversalTime() const { return fUniversalTime; }
