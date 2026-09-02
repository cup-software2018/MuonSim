#include <iostream>

#include "MCPrimary.hh"
#include "MCPrimaryData.hh"

ClassImp(MCPrimaryData)

MCPrimaryData::MCPrimaryData()
  : TClonesArray("MCPrimary")
{
}

MCPrimaryData::MCPrimaryData(const MCPrimaryData & data)
  : TClonesArray(data)
{
}

MCPrimaryData::~MCPrimaryData() = default;

// See the note in MCTrackData::Add.
MCPrimary * MCPrimaryData::Add()
{
  return static_cast<MCPrimary *>(ConstructedAt(GetEntriesFast(), "C"));
}

void MCPrimaryData::Clear(Option_t * opt) { TClonesArray::Clear("C"); }

void MCPrimaryData::Print(Option_t * opt) const
{
  int n = GetN();
  std::cout << "===> MCPrimaryData: number of primary: " << n << "\n";
  for (auto obj : *this) {
    static_cast<MCPrimary *>(obj)->Print();
  }
}
