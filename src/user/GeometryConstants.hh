#pragma once

#include "CLHEP/Units/SystemOfUnits.h"
#include "G4Types.hh"

// Every dimension the geometry is built from, in one place.
//
// This is survey data, not code: each number below is something somebody measured
// or specified, and correcting one should mean editing one line here rather than
// hunting through DetectorConstruction.cc. Anything derived from these -- a centre
// computed from a bottom, a water level computed from a PMT length -- stays in the
// builder, because that is a consequence rather than a measurement.
//
// PROVISIONAL marks a number that is a placeholder waiting for real survey data.
namespace geo
{

using CLHEP::cm;
using CLHEP::deg;
using CLHEP::mm;

// ---------------------------------------------------------------------------
// The hall: a sphere of rock with an air cavern hollowed out of its top.
// ---------------------------------------------------------------------------
namespace hall
{

// The hall: the air the detectors stand in. FIXED.
//
// Every detector position is measured against this and against the floor below, so
// it is an input, not something derived from how much rock is wanted. Changing it
// moves the walls in on the apparatus.
constexpr G4double kHallRadius = 15000. * mm;

// The hall floor -- the datum every detector structure stands on, and the world's
// equatorial plane, so every height in the hall is simply its height above the floor.
//
constexpr G4double kFloorZ = 0.;

// How much rock. ADJUSTABLE, and uniform: the rock is the hall's own shape grown
// outward by this, so the dome, the floor and the pit walls all carry the same
// thickness. The world grows to hold it rather than the hall shrinking to make room.
//
// Beyond the shell there is no rock, so what a muon crossing the floor can produce
// is limited to this depth. One metre is 5 to 10 attenuation lengths for the gammas
// and fast neutrons that would come back out, so a thicker shell adds rock that
// contributes exponentially little -- but it does lengthen the muon's path in rock,
// which is the reason to raise it.
//
// It is also what makes a rock-gamma run efficient: at 1 m the whole rock IS the
// layer decays can escape from, so "involume Rock" wastes nothing.
constexpr G4double kRockThickness = 1000. * mm;

// Air outside the rock, so the rock shell does not end on the world's own surface.
constexpr G4double kWorldMargin = 500. * mm;

constexpr G4double kWorldRadius = kHallRadius + kRockThickness + kWorldMargin;

// ASSUMED: "standard rock", the convention underground muon work uses. The site's
// own composition and density belong here.
constexpr G4double kRockDensity = 2.65 * CLHEP::g / CLHEP::cm3;

// A rectangular pit sunk into the hall floor, directly under the shielding. Empty
// -- it is excavated rock, so what fills it is air.
namespace pit
{

constexpr G4double kDepth = 5000. * mm; // below the cavern floor, so its bottom is at -5000

// Footprint: big enough for what goes in it. The 22 floor PSMD modules lie 11 across
// by 2 deep, which is 11 x 310 = 3410 by 2 x 1680 = 3360 mm -- wider than the
// shielding's own 3320 in both directions, so the excavation is set by the modules
// rather than by the shielding above. Still PROVISIONAL: this is the minimum that
// holds them, with no working clearance, and the real excavation is a survey number.
constexpr G4double kHalfX = 1705. * mm;
constexpr G4double kHalfY = 1680. * mm;

constexpr G4double kTopZ = kFloorZ;
constexpr G4double kBottomZ = kFloorZ - kDepth;

} // namespace pit

} // namespace hall

// ---------------------------------------------------------------------------
// The steel frame carrying the WCMD, and defining the detector room under it.
// ---------------------------------------------------------------------------
namespace frame
{

// An H-beam's cross section, named as the structural tables name it:
// H x B x t1 x t2, e.g. H-300x300x10x15.
struct HBeamSection {
  G4double H;  // depth, flange outer face to flange outer face
  G4double B;  // flange width
  G4double t1; // web thickness
  G4double t2; // flange thickness
};

// PROVISIONAL: H-300x300x10x15, a common column section, 92.1 kg/m by these
// dimensions (catalogues say 94.0, the difference being the web-to-flange
// fillets, which are not modelled).
//                                    H          B          t1        t2
constexpr HBeamSection kSection = {300. * mm, 300. * mm, 10. * mm, 15. * mm};

// Top of the frame: the surface the WCMD stands on.
//
// A survey number, so it is independent rather than derived. It is constrained from
// below: the shielding is 3865 mm tall standing on the cavern floor, and the top
// girders are one section deep and have to clear it, which puts the lowest allowed
// value at 3865 + 300 = 4165. This clears that by 385 mm.
//
// Raising this raises the tank with it, since wcmd::kBottomZ is defined from it. It
// also moves the farthest detector point, so the cosmic generator's virtual sphere
// has to be re-derived -- macro/cosmic_muon.mac carries that calculation, and a
// sphere left too small drops muons that should have been generated without saying
// so.
constexpr G4double kTopZ = 4550. * mm;

// PROVISIONAL: the columns stand on a square matching the tank they carry.
constexpr G4double kHalfSpan = 3205. * mm;

} // namespace frame

// ---------------------------------------------------------------------------
// WCMD -- the water Cherenkov muon detector.
// ---------------------------------------------------------------------------
namespace wcmd
{

// The tank stands on the frame, so this is not a free parameter.
constexpr G4double kBottomZ = frame::kTopZ;

constexpr G4double kTankHalfXY = 3205. * mm; // steel shell, outer
constexpr G4double kTankHalfZ = 1571. * mm;  // steel shell Z half; the door spans it too
constexpr G4double kCavHalfXY = 3197. * mm;  // inside the shell, i.e. an 8 mm wall
constexpr G4double kCavHalfZ = 1563. * mm;

constexpr G4double kWaterHalfZ = 1433. * mm;
constexpr G4double kPMTTopGap = 1. * mm; // water surface to tank ceiling

// Door on the +Y face, at the -X corner. Outer box and its hollow.
constexpr G4double kDoorHalfX = 877.5 * mm;
constexpr G4double kDoorHalfY = 350. * mm;
constexpr G4double kDoorInnerHalfX = 869.5 * mm;
constexpr G4double kDoorInnerHalfY = 342. * mm;
constexpr G4double kDoorInnerHalfZ = 1563. * mm;
constexpr G4double kDoorX = -2327.5 * mm;
constexpr G4double kDoorY = 2855. * mm;

// The dry inner room, in the TankAir frame.
constexpr G4double kRoomOuterHalfXY = 2513. * mm;
constexpr G4double kRoomOuterHalfZ = 1083. * mm;
constexpr G4double kRoomInnerHalfXY = 2505. * mm;
constexpr G4double kRoomInnerHalfZ = 1075. * mm;
constexpr G4double kRoomZ = -480. * mm;

constexpr int kNPMT = 48;

// PMT 47 sits inside the door rather than in the tank proper, which is why it is
// placed in a different mother and paired with DoorWater.
constexpr int kDoorPMT = 47;

// PMT (x, y) in the TankAir frame [mm].
constexpr G4double kPMTxy[kNPMT][2] = {
    {2850, -2850},  {1900, -2850},  {950, -2850},  {0, -2850},   {-950, -2850}, {-1900, -2850},
    {-2850, -2850}, {2850, -1900},  {1900, -1900}, {950, -1900}, {0, -1900},    {-950, -1900},
    {-1900, -1900}, {-2850, -1900}, {2850, -950},  {1900, -950}, {950, -950},   {0, -950},
    {-950, -950},   {-1900, -950},  {-2850, -950}, {2850, 0},    {1900, 0},     {950, 0},
    {0, 0},         {-950, 0},      {-1900, 0},    {-2850, 0},   {2850, 950},   {1900, 950},
    {950, 950},     {0, 950},       {-950, 950},   {-1900, 950}, {-2850, 950},  {2850, 1900},
    {1900, 1900},   {950, 1900},    {0, 1900},     {-950, 1900}, {-1900, 1900}, {-2850, 1900},
    {2850, 2850},   {1900, 2850},   {950, 2850},   {0, 2850},    {-950, 2850},  {-2328, 2850}};

} // namespace wcmd

// ---------------------------------------------------------------------------
// PSMD -- the plastic scintillator muon detector.
// ---------------------------------------------------------------------------
namespace psmd
{

// Module numbers run 1..kModules as the survey drawing labels them, and the
// numbering has holes: twelve of them are not installed. Each module holds two
// scintillator slabs, and a slab is what deposits energy, so the identifier the
// simulation carries is a SLAB id:
//   id = kSlabsPerModule * module + slab,  slab 0 = upper, 1 = lower
// which maps straight back: module = id / 2, slab = id % 2. That leaves id 0 and 1
// unused, module 0 not existing, hence the +1 in the bound.
//
// Not to be confused with a readout channel. The drawing reads each slab at both
// ends, so one module has four channels and one slab has two; the end a photon
// left by is not recoverable from energy alone, and no readout device is built
// yet. "channel" is kept for that level, when it comes.
constexpr int kModules = 130;
constexpr int kSlabsPerModule = 2;
constexpr int kSlabIds = (kModules + 1) * kSlabsPerModule;

// One module: an aluminium box holding two scintillator slabs.
constexpr G4double kAlHalfX = 840. * mm;
constexpr G4double kAlHalfY = 155. * mm;
constexpr G4double kAlHalfZ = 30. * mm;
constexpr G4double kWall = 1. * mm; // aluminium thickness
constexpr G4double kGap = 1. * mm;  // slab to inner wall face

constexpr G4double kScintHalfX = 835. * mm;
constexpr G4double kScintHalfY = 151.5 * mm;
constexpr G4double kScintHalfZ = 7.75 * mm;

// The modules standing against the shielding's four side faces.
//
// A group is kModulesPerGroup modules stacked one above the other with their long
// side horizontal, so a group is one module long and twelve modules tall. Two
// groups to a face, four faces: kWallModules of them. Each module's upper slab
// faces outward, away from the shielding.
//
// The two groups on a face do not meet in the middle; the leftover width shows up
// as a gap there. How much is left over differs between the two pairs of faces,
// because of how the corners are arranged: the y-face groups run through, covering
// the corner out to the outer surface of the x-face groups, while the x-face groups
// stop against the inner surface of the y-face ones. So the y faces are the wider
// span and carry the bigger middle gap.
namespace wall
{

constexpr G4double kGap = 50. * mm; // module inner face to the HDPE face
constexpr int kModulesPerGroup = 12;
constexpr int kGroupsPerFace = 2;
constexpr int kWallModules = 4 * kGroupsPerFace * kModulesPerGroup;

} // namespace wall

// The modules lying in the pit under the shielding: 11 across in x, 2 deep in y,
// long side along y. Their upper slab faces DOWN, away from the shielding, which is
// the same sense as the wall groups: everywhere in the PSMD, PSMDScintTop looks away
// from what it shields.
//
// Readout, from the drawing: each slab is read at both ends, so a module has four
// channels -- top panel ch3/ch4, bottom panel ch1/ch2, with ch3/ch1 at the +y end
// and ch4/ch2 at the -y end for both rows. Only the slab, not the end, is
// distinguishable from energy alone: the two ends of one slab see the same deposit,
// and what separates them is attenuation and propagation delay along the 1670 mm
// bar, which needs the optical model.
namespace floorLayer
{

constexpr G4double kGapBelowShield = 400. * mm; // HDPE underside to the module top face
constexpr int kColumns = 11;                    // along x
constexpr int kRows = 2;                        // along y
constexpr int kFloorModules = kColumns * kRows;

} // namespace floorLayer

} // namespace psmd

// ---------------------------------------------------------------------------
// The passive shielding: four nested layers around a cavity open at the top.
// ---------------------------------------------------------------------------
namespace shield
{

constexpr G4double kCavityHalfX = 700. * mm; // 1400 mm across
constexpr G4double kCavityHalfY = 700. * mm;
constexpr G4double kCavityH = 2905. * mm;

// Layer thicknesses, outermost first.
constexpr G4double kHDPE = 700. * mm;
constexpr G4double kBoratedRubber = 10. * mm;
constexpr G4double kLead = 200. * mm;
constexpr G4double kInner = 50. * mm; // lead or copper

// Overshoot on the box that cuts each layer's cavity, so the open top is a clean
// face rather than two solids ending on exactly the same plane.
constexpr G4double kCutOvershoot = 1. * mm;

// On the hall axis. The shielding is the thing everything else is arranged around,
// so it does not get moved out of the way -- it was offset for a while, back when
// the water tank's underside was at z = 4000 and the shielding's top ran into it.
// The tank now stands on the frame, whose top is kTopZ, and the frame's columns are
// out at 3205, so nothing is in the way of the shielding or of the PSMD panels that
// reach 1770 mm from this axis.
//
// The top is at kFloorZ + (kHDPE + kBoratedRubber + kLead + kInner) + kCavityH
// = 3865 mm, which is 685 mm clear of the frame top. Written as the sum rather
// than as a number because the number went stale twice: the comment above said
// 4865 while kCavityH was still 3905, and 5300 for a frame top that is now 4550.
constexpr G4double kX = 0. * mm;
constexpr G4double kY = 0. * mm;

// ASSUMED borated rubber: 5% boron by mass at 1.5 g/cm3. Both are placeholders in
// the range commercial sheets are sold in (1-5% B, 1.2-1.8 g/cm3). The boron is
// what the layer is for, so its mass fraction is the number that matters.
constexpr G4double kRubberDensity = 1.5 * CLHEP::g / CLHEP::cm3;
constexpr G4double kRubberBoronFraction = 0.05;

} // namespace shield

} // namespace geo
