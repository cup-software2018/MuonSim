#pragma once

#include "TClonesArray.h"

#include "MCScint.hh"

// Every scintillator that saw energy in this event, sorted by detector ID.
// Ones that saw nothing are absent rather than present with zero.
class MCScintData : public TClonesArray {
public:
  MCScintData();
  MCScintData(const MCScintData & data);
  virtual ~MCScintData();

  void Clear(Option_t * opt = "") override;

  MCScint * Add();

  int GetN() const;
  MCScint * Get(int i) const;
  MCScint * FindScint(int id) const;

  void Print(Option_t * opt = "") const override;

  ClassDefOverride(MCScintData, 1)
};

inline int MCScintData::GetN() const { return GetEntriesFast(); }

inline MCScint * MCScintData::Get(int n) const { return static_cast<MCScint *>(At(n)); }
