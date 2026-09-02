#include "MCScint.hh"
#include "MCScintData.hh"

ClassImp(MCScintData)

MCScintData::MCScintData()
  : TClonesArray("MCScint")
{
}

MCScintData::MCScintData(const MCScintData & data)
  : TClonesArray(data)
{
}

MCScintData::~MCScintData() = default;

// See the note in MCTrackData::Add.
MCScint * MCScintData::Add()
{
  return static_cast<MCScint *>(ConstructedAt(GetEntriesFast(), "C"));
}

void MCScintData::Clear(Option_t *) { TClonesArray::Clear("C"); }

MCScint * MCScintData::FindScint(int id) const
{
  for (auto obj : *this) {
    auto scint = static_cast<MCScint *>(obj);
    if (scint->GetId() == id) return scint;
  }
  return nullptr;
}

void MCScintData::Print(Option_t *) const
{
  for (auto obj : *this) {
    static_cast<MCScint *>(obj)->Print();
  }
}
