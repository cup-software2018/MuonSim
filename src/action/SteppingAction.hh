#pragma once

#include <functional>
#include <vector>

#include "G4Types.hh"
#include "G4UserSteppingAction.hh"

class G4Step;
class RootManager;
class SteppingObserver;

// Records every step, and optionally tags which region of the detector, and
// which readout element, the step was in.
//
// Both taggings are hooks rather than built in: what counts as a "region", which
// volumes bound it, which particles are worth tagging and how a readout element
// is numbered are all properties of a particular detector, so the geometry
// supplies the judgement and this class stays geometry-independent. Construct
// without a tagger and that tagging simply does not happen.
class SteppingAction : public G4UserSteppingAction {
public:
  // Returns the region bits for this step, or 0 for none. The meaning of the
  // bits is entirely up to whoever supplies the tagger; they are stored verbatim
  // in MCTrack::fRegionMask.
  using RegionTagger = std::function<G4int(const G4Step *)>;

  // Returns the ID of the readout element the step happened in, or -1 for none.
  // A name is not enough to identify one: a module built as a single logical
  // volume and placed 130 times shares one name across all 130 copies, so the
  // ID has to come from the copy numbers along the touchable history -- which
  // only the geometry knows how to read. Stored verbatim in MCStep::fDetectorID.
  using DetectorIDTagger = std::function<G4int(const G4Step *)>;

  explicit SteppingAction(RootManager * rootManager, RegionTagger tagger = {},
                          DetectorIDTagger idTagger = {},
                          std::vector<SteppingObserver *> observers = {});
  ~SteppingAction() override = default;

  void UserSteppingAction(const G4Step * step) override;

private:
  RootManager * fRootManager = nullptr;
  RegionTagger fRegionTagger;
  DetectorIDTagger fDetectorIDTagger;

  // Not owned: ActionInitialization keeps them alive for the run.
  std::vector<SteppingObserver *> fObservers;
};
