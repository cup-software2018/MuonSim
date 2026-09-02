#include "MCTrack.hh"
#include "MCTrackData.hh"

ClassImp(MCTrackData)

MCTrackData::MCTrackData()
  : TClonesArray("MCTrack")
{
}

MCTrackData::MCTrackData(const MCTrackData & data)
  : TClonesArray(data)
{
}

MCTrackData::~MCTrackData() = default;

// ConstructedAt(n, "C") rather than placement-new over slot n.
//
// `new ((*this)[n]) T()` builds a new object on top of a LIVE one without running
// its destructor, so every std::string and std::vector it owned is leaked -- once
// per entry, per event. That cost about 150 kB an event here, RSS climbing without
// bound. ConstructedAt constructs a slot only the first time it is used and calls
// the object's Clear() on reuse, which is what makes the reuse safe.
//
// The bargain is that Clear() must reset EVERY member: the constructor does not
// run again, so anything Clear() forgets carries over from the previous event.
MCTrack * MCTrackData::Add()
{
  return static_cast<MCTrack *>(ConstructedAt(GetEntriesFast(), "C"));
}

MCTrack * MCTrackData::FindTrack(int id) const
{
  for (auto obj : *this) {
    auto track = static_cast<MCTrack *>(obj);
    if (track->GetTrackId() == id) return track;
  }
  return nullptr;
}

MCTrack * MCTrackData::GetParentTrack(MCTrack * track) const
{
  return FindTrack(track->GetParentId());
}

void MCTrackData::Clear(Option_t * opt) { TClonesArray::Clear("C"); }

void MCTrackData::Print(Option_t * opt) const
{
  for (auto obj : *this) {
    static_cast<MCTrack *>(obj)->Print();
  }
}
