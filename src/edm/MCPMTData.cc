#include "MCPMT.hh"
#include "MCPMTData.hh"

ClassImp(MCPMTData)

MCPMTData::MCPMTData()
  : TClonesArray("MCPMT")
{
}

MCPMTData::MCPMTData(const MCPMTData & data)
  : TClonesArray(data)
{
}

MCPMTData::~MCPMTData() = default;

// See the note in MCTrackData::Add.
MCPMT * MCPMTData::Add()
{
  return static_cast<MCPMT *>(ConstructedAt(GetEntriesFast(), "C"));
}

void MCPMTData::Clear(Option_t * opt) { TClonesArray::Clear("C"); }

void MCPMTData::Print(Option_t * opt) const
{
  for (auto obj : *this) {
    static_cast<MCPMT *>(obj)->Print();
  }
}
