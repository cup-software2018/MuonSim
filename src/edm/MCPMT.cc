#include <algorithm>
#include <iostream>

#include "MCPMT.hh"

ClassImp(MCPMT)

MCPMT::MCPMT()
  : TObject()
{
}

MCPMT::MCPMT(int id)
  : TObject(),
    fPMTId(id)
{
}

MCPMT::MCPMT(const MCPMT & pmt)
  : TObject(pmt),
    fPMTId(pmt.GetId()),
    fHits(pmt.fHits)
{
}

MCPMT::~MCPMT() = default;

MCPhotonHit * MCPMT::AddHit()
{
  fHits.emplace_back();
  return &fHits.back();
}

MCPhotonHit * MCPMT::AddHit(MCPhotonHit * hit)
{
  fHits.push_back(*hit);
  return &fHits.back();
}

void MCPMT::Clear(Option_t * opt)
{
  fPMTId = -1;
  fHits.clear();
}

void MCPMT::Sort()
{
  std::sort(fHits.begin(), fHits.end(),
            [](const MCPhotonHit & a, const MCPhotonHit & b) { return a.GetTime() < b.GetTime(); });
}

int MCPMT::Compare(const TObject * object) const
{
  auto comp = static_cast<const MCPMT *>(object);
  if (this->GetId() > comp->GetId()) return 1;
  if (this->GetId() < comp->GetId()) return -1;
  return 0;
}

void MCPMT::Print(Option_t * opt) const
{
  std::cout << "PMTID=" << fPMTId << ", # of hit=" << GetNHit() << std::endl;
}
