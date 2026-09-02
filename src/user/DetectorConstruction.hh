#pragma once

#include <functional>
#include <set>
#include <vector>

#include "GeometryConstants.hh"

#include "G4ThreeVector.hh"
#include "G4Types.hh"
#include "G4VUserDetectorConstruction.hh"

class G4VPhysicalVolume;
class G4LogicalVolume;
class G4Step;
class G4OpticalSurface;

class DetectorConstruction : public G4VUserDetectorConstruction {
public:
  // Which water region a muon was crossing. These bits are this detector's own
  // definition; they are stored verbatim in MCTrack::fRegionMask, so nothing
  // outside this class needs to know what they mean.
  enum WCMDRegion : int {
    kRegNone    = 0,
    kRegCeiling = 1 << 0, // water above the inner room
    kRegWallPX  = 1 << 1,
    kRegWallMX  = 1 << 2,
    kRegWallPY  = 1 << 3,
    kRegWallMY  = 1 << 4,
    kRegDoor    = 1 << 5
  };

  // Bounds of the inner room in TankWater-local coordinates.
  // Used by SteppingAction to classify which wall/ceiling a muon is traversing.
  struct WaterRegionBounds {
    double roomHalfX = 0;  // half-width of inner room in X [mm]
    double roomHalfY = 0;  // half-width of inner room in Y [mm]
    double roomTopZ  = 0;  // Z of room top face in TankWater local frame [mm]
  };

  // One member of the frame. A member runs along `axis` for `length`, centred on
  // `centre`, both in the mother's frame. `roll` turns the section about the
  // member's own axis, which is what decides whether the web stands up or lies
  // flat -- structurally the whole point of the section, though for the mass a
  // muon sees it makes no difference.
  struct HBeamMember {
    G4ThreeVector centre;
    G4double      length;
    G4ThreeVector axis = G4ThreeVector(0., 0., 1.);
    G4double      roll = 0.;
  };

  // What the innermost shielding layer is made of. Lead continues the layer
  // outside it; copper is the low-background alternative, chosen when the
  // shielding's own gamma activity next to the detector is what matters.
  enum class ShieldInner { Lead, Copper };

  // Where one PSMD module sits, and which way it faces.
  //
  // `normal` is the direction the module's upper slab (PSMDScintTop) looks. That is
  // the natural way to say it, because it is how the modules are specified: lying
  // flat with the upper plastic up is normal = +z; standing against the +x face of
  // something with the upper plastic outward is normal = +x. A rotation about the
  // vertical alone cannot express the second, which is why this is a direction
  // rather than an angle.
  //
  // `roll` then turns the module about that normal, choosing which way its long
  // (1680 mm) axis runs. Measured, not assumed:
  //
  //   normal = +z (flat)  roll = 0       -> long axis along x
  //   normal = +x (wall)  roll = 0       -> long axis VERTICAL
  //                       roll = 90 deg  -> long axis horizontal, 310 mm side upright
  //
  // The wall case is the one that surprises: swinging local +z onto a horizontal
  // normal carries the long axis into the vertical, so a module lying across a wall
  // with its short side upright needs roll = 90 degrees.
  // `id` is the detector's own module number, 1..geo::psmd::kModules, as the survey
  // drawing labels them -- not this table's row index. The numbering has holes in
  // it: twelve numbers belong to modules that are not installed, so an index would
  // not agree with what anyone calls a module.
  struct PSMDPlacement {
    int           id = 0;
    G4ThreeVector centre;
    G4ThreeVector normal = G4ThreeVector(0., 0., 1.);
    G4double      roll   = 0.;
  };

  // Detector IDs for the PSMD run
  //   id = geo::psmd::kSlabsPerModule * module + slab,  0 <= id < geo::psmd::kSlabIds
  // with module the number the survey drawing gives and slab 0 = top, 1 = bottom.
  // The id names a SLAB, not a readout channel -- see geo::psmd. Read the pair as
  // one module by taking id / geo::psmd::kSlabsPerModule.

  DetectorConstruction() = default;
  ~DetectorConstruction() override = default;

  // Reserved for geometry-from-YAML, which has no loader yet. Construct() refuses
  // to run when one is set rather than ignoring it: a file the user believes is in
  // effect but which nothing reads is a worse trap than the missing feature.
  void SetGeometryFile(const G4String & path) { fGeometryFile = path; }

  G4VPhysicalVolume * Construct() override;

  G4LogicalVolume *       GetScoringVolume()      const { return fScoringVolume; }
  const WaterRegionBounds & GetWaterRegionBounds() const { return fWaterRegionBounds; }

  // The region tagger to hand to SteppingAction. Holding the volume names and
  // room dimensions here is what keeps SteppingAction geometry-independent.
  std::function<G4int(const G4Step *)> MakeRegionTagger() const;

  // The detector-ID tagger to hand to SteppingAction: which PSMD slab a step
  // deposited its energy in, or -1 if it was not in one.
  std::function<G4int(const G4Step *)> MakeDetectorIDTagger() const;

private:
  void DefineMaterials();
  void DefineOpticalSurfaces();

  // The experimental hall everything sits in.
  G4VPhysicalVolume * BuildWorld();

  // The two muon sub-detectors. Each places itself into the world it is given,
  // so they stay independent of one another.
  void BuildWCMD(G4LogicalVolume * world, G4double bottomZ);
  void BuildPSMD(G4LogicalVolume * world, const std::vector<PSMDPlacement> & placements);

  // One PSMD module: an aluminium box holding two plastic scintillator slabs.
  // All 130 modules are identical, so this logical volume is built once and
  // placed repeatedly.
  G4LogicalVolume * BuildPSMDModule();

  // Where the modules go. Survey data, kept apart from the module's own
  // construction so that re-surveying touches nothing but this table.
  static std::vector<PSMDPlacement> PSMDLayout();

  // The floor layer lives in the pit, a different mother from the wall groups, so it
  // is a separate table -- and its coordinates are in the pit's frame.
  static std::vector<PSMDPlacement> PSMDFloorLayout();

  // One H-beam member as a logical volume: an exact H cross section extruded
  // along its own axis. Built per distinct length, since that is what changes.
  G4LogicalVolume * MakeHBeam(const G4String & name, const geo::frame::HBeamSection & section,
                              G4double length);

  // The steel frame that carries the WCMD and defines the detector room. Every
  // member comes from the table, so re-surveying the frame touches nothing else.
  void BuildHBeamFrame(G4LogicalVolume * mother, const geo::frame::HBeamSection & section,
                       const std::vector<HBeamMember> & members);

  // Where the members are. Survey data.
  static std::vector<HBeamMember> HBeamFrameLayout();

  // The passive shielding around the detector: four nested layers, open at the
  // top. bottomCentre is the centre of its underside IN THE MOTHER'S FRAME -- the
  // point it stands on -- so the caller decides both where it sits and what it
  // sits in.
  void BuildShielding(G4LogicalVolume * mother, const G4ThreeVector & bottomCentre,
                      ShieldInner inner = ShieldInner::Lead);

  // optical surfaces
  G4OpticalSurface * fTyvekSurface        = nullptr;
  G4OpticalSurface * fPhotocathodeSurface = nullptr;
  G4OpticalSurface * fWaterPMTSurface     = nullptr;

  G4LogicalVolume *  fScoringVolume      = nullptr;
  G4LogicalVolume *  fRoomAirVolume      = nullptr;
  G4LogicalVolume *  fHallVolume         = nullptr; // the air the detectors live in
  G4String           fGeometryFile;
  std::set<int>      fPSMDIds;                      // every module number placed so far
  WaterRegionBounds  fWaterRegionBounds  = {};
};
