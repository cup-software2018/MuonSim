#include <iostream>

#include "MCEventInfo.hh"

ClassImp(MCEventInfo)

MCEventInfo::MCEventInfo()
  : TObject()
{
}

MCEventInfo::MCEventInfo(const MCEventInfo & info)
  : TObject(),
    fEventNumber(info.GetEventNumber()),
    fUniversalTime(info.GetUniversalTime())
{
}

MCEventInfo::~MCEventInfo() = default;

void MCEventInfo::Print(Option_t * opt) const
{
  std::cout << "++++++++++++++++ Event No: " << fEventNumber << "  t = " << fUniversalTime
            << " s ++++++++++++++++++\n";
}
