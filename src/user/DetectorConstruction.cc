#include <cmath>
#include <set>
#include <string>

#include "DetectorConstruction.hh"
#include "GeometryConstants.hh"
#include "DataPath.hh"
#include "MaterialPropertyFile.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4Box.hh"
#include "G4Exception.hh"
#include "G4IntersectionSolid.hh"
#include "G4LogicalBorderSurface.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4MultiUnion.hh"
#include "G4NistManager.hh"
#include "G4OpticalSurface.hh"
#include "G4Orb.hh"
#include "G4PVPlacement.hh"
#include "G4RotationMatrix.hh"
#include "G4RunManager.hh"
#include "G4SDManager.hh"
#include "G4SubtractionSolid.hh"
#include "G4UnionSolid.hh"
#include "G4SystemOfUnits.hh"
#include "G4Transform3D.hh"
#include "G4Tubs.hh"
#include "G4VisAttributes.hh"
#include "PMTLogicalVolume.hh"
#include "PMTSD.hh"


namespace
{
// The optical properties this detector was built around. Overridden by -m or
// /material/load; resolved through the data search path when neither is given.
constexpr const char * kDefaultMaterialFile = "optical/wcmd_materials.yml";
} // namespace

void DetectorConstruction::DefineMaterials()
{
  auto nist = G4NistManager::Instance();
  nist->FindOrBuildMaterial("G4_WATER");
  nist->FindOrBuildMaterial("G4_Pyrex_Glass");
  // remaining materials (air, steel, vacuum, aluminum) need no optical properties
  nist->FindOrBuildMaterial("G4_AIR");
  nist->FindOrBuildMaterial("G4_STAINLESS-STEEL");
  nist->FindOrBuildMaterial("G4_Galactic");
  nist->FindOrBuildMaterial("G4_Al");

  // The PSMD scintillator. No SCINTILLATIONYIELD or RINDEX yet, so G4Scintillation
  // makes no photons: the slab is read as energy, with the Birks-quenched
  // deposit standing in for the light yield. Once the optical properties are in,
  // "/process/optical/processActivation Scintillation false" is what switches the
  // photons back off for a fast run -- it changes nothing about the energy, which
  // the ionisation processes deposit either way.
  //
  // The Birks constant has to be set by hand. Geant4's built-in table
  // (G4EmSaturation) covers G4_POLYSTYRENE at 0.07943 mm/MeV but NOT
  // G4_PLASTIC_SC_VINYLTOLUENE, which arrives with 0 -- and a zero constant makes
  // the quenched deposit identical to the raw one. 0.126 mm/MeV is the value
  // Geant4's own examples use for plastic scintillator (TestEm3, Hadr05, amsEcal,
  // wls). For minimum-ionising muons the quenching is well under a percent; it is
  // alphas, protons and nuclear recoils that this number matters to.
  nist->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");

  // ---- shielding materials ----
  // G4_POLYETHYLENE is 0.94 g/cm3, the low end of HDPE (0.94-0.97): fine for a
  // neutron moderator, but it is the generic polyethylene, not an HDPE entry.
  nist->FindOrBuildMaterial("G4_POLYETHYLENE");
  nist->FindOrBuildMaterial("G4_Pb");
  nist->FindOrBuildMaterial("G4_Cu");
  // Structural steel for the H-beam frame. Geant4 has no steel entry other than
  // G4_STAINLESS-STEEL, which is the tank's Fe/Cr/Ni at 8.0 g/cm3 and not what a
  // frame is made of; G4_Fe is plain iron at 7.874. Carbon steel carries ~0.2 wt%
  // carbon, so SS400/SM490 differs from pure iron by well under a percent in both
  // composition and density -- nothing a muon or a gamma distinguishes. What does
  // matter about frame steel is its contamination (60Co, 238U/232Th), and that
  // belongs to the generator, not the material: "60Co multivolume HBeam".
  nist->FindOrBuildMaterial("G4_Fe");

  // The cavern wall. ASSUMED: "standard rock" -- SiO2 composition at 2.65 g/cm3,
  // the convention underground-muon work uses. The site's own rock composition and
  // density belong here; they set how much of the muon flux and the rock gamma and
  // neutron yield actually reaches the cavern.
  auto sio2 = nist->FindOrBuildMaterial("G4_SILICON_DIOXIDE");
  auto rock = new G4Material("Rock", geo::hall::kRockDensity, 1);
  rock->AddMaterial(sio2, 1.0);

  // Borated rubber is not a NIST material, so it is mixed here.
  //
  // ASSUMED COMPOSITION -- 5% boron by mass in natural rubber at 1.5 g/cm3. Both
  // numbers are placeholders in the range commercial sheets are sold in (1-5% B,
  // 1.2-1.8 g/cm3); the real sheet's boron loading and density belong here. The
  // boron is what the layer is for, so its mass fraction is the number that
  // matters -- thermal neutron capture on 10B goes as the areal boron density.
  const G4double kBoronMassFraction = geo::shield::kRubberBoronFraction;
  auto rubber = nist->FindOrBuildMaterial("G4_RUBBER_NATURAL");
  auto boratedRubber = new G4Material("BoratedRubber", geo::shield::kRubberDensity, 2);
  boratedRubber->AddMaterial(rubber, 1. - kBoronMassFraction);
  boratedRubber->AddElement(nist->FindOrBuildElement("B"), kBoronMassFraction);

  // Optical properties -- refractive indices, absorption and Rayleigh lengths --
  // are not here: they come from the material-property file, which is applied once
  // the surfaces below exist too. See src/phys/MaterialPropertyFile.hh.
}


void DetectorConstruction::DefineOpticalSurfaces()
{
  // ---- Tyvek reflector ----
  fTyvekSurface = new G4OpticalSurface("TyvekSurface");
  fTyvekSurface->SetType(dielectric_metal);
  fTyvekSurface->SetFinish(ground);
  fTyvekSurface->SetModel(unified);


  // ---- photocathode (thin-film model) ----
  fPhotocathodeSurface = new G4OpticalSurface("Photocathode_opsurf");
  fPhotocathodeSurface->SetType(dielectric_metal);
  fPhotocathodeSurface->SetFinish(polished);
  fPhotocathodeSurface->SetModel(glisur);


  // ---- transparent water↔PMT border (overrides Tyvek skin at PMT hole faces) ----
  fWaterPMTSurface = new G4OpticalSurface("WaterPMTSurf");
  fWaterPMTSurface->SetType(dielectric_dielectric);
  fWaterPMTSurface->SetFinish(polished);
  fWaterPMTSurface->SetModel(glisur);
}


G4VPhysicalVolume * DetectorConstruction::Construct()
{
  // Refuse rather than ignore: a geometry file the user believes is in effect,
  // silently read by nothing, would put every dimension in this run in doubt.
  if (!fGeometryFile.empty()) {
    G4Exception("DetectorConstruction::Construct", "GEO005", FatalException,
                ("geometry from YAML is not implemented -- '" + fGeometryFile +
                 "' would be ignored. Dimensions come from GeometryConstants.hh.")
                    .c_str());
  }

  DefineMaterials();
  DefineOpticalSurfaces();

  // The muon detector is WCMD + PSMD. Each sub-detector builds itself into the
  // volume it is given, so adding one does not disturb the other.
  //
  // They go in the cavern, not the world: the world is now mostly rock. The cavern
  // sits at the origin, so its coordinates are the world's.
  // Optical properties from file, applied now that the materials and surfaces the
  // file names exist. Whatever the file sets overrides the built-in tables, so a
  // run without one still works and a run with one need only carry what it changes.
  // Nothing named on the command line or in the macro: fall back to the file that
  // ships with the installation. It is not optional -- without it the water has no
  // refractive index and the detector makes no light at all -- so a missing file is
  // a fatal error rather than a quiet run with no Cherenkov.
  if (RegisteredMaterialPropertyFiles().empty()) {
    const std::string fallback = ResolveDataFile(kDefaultMaterialFile);
    if (fallback.empty()) {
      G4Exception("DetectorConstruction::Construct", "GEO004", FatalException,
                  ("cannot find " + std::string(kDefaultMaterialFile) +
                   " -- name one with -m, or set MUONSIM_DATA")
                      .c_str());
    }
    RegisterMaterialPropertyFile(fallback);
  }

  std::string materialError;
  if (!ApplyMaterialPropertyFiles(materialError)) {
    G4Exception("DetectorConstruction::Construct", "GEO003", FatalException,
                materialError.c_str());
  }

  auto * physWorld = BuildWorld();

  // Where the WCMD stands. Its x and y are fixed on the hall axis; the floor
  // height is the survey number, so that is what gets passed in.
  // The frame first: the WCMD stands on it, so its top is the tank's underside.
  BuildHBeamFrame(fHallVolume, geo::frame::kSection, HBeamFrameLayout());

  BuildWCMD(fHallVolume, geo::wcmd::kBottomZ);

  BuildPSMD(fHallVolume, PSMDLayout());

  // The floor layer, in the pit rather than the cavern.
  BuildPSMD(fHallVolume, PSMDFloorLayout());

  // The shielding stands on the cavern floor, i.e. on the rock.
  //
  // It cannot go in the WCMD's dry inner room: the shielding is 3865 mm tall and
  // RoomAir is 2150 mm, so it would protrude 1715 mm through the room ceiling. It
  // is in fact taller than the whole water tank (3142 mm), so no arrangement puts
  // it inside the WCMD as the tank is currently dimensioned.
  //
  // The shielding, and the 96 PSMD modules that stand against its four faces.
  //
  // Both read their position from geo::shield::kX/kY -- PSMDLayout works out where
  // the panels go from the same constants. They have to agree: a placement written
  // out by hand here would leave the panels somewhere the shielding is not, and
  // nothing would complain, since two structures in clear space overlap nothing.
  BuildShielding(fHallVolume,
                 G4ThreeVector(geo::shield::kX, geo::shield::kY, geo::hall::kFloorZ));

  return physWorld;
}


// The experimental hall: a sphere of rock with an air cavern hollowed out of its
// top half.
//
G4VPhysicalVolume * DetectorConstruction::BuildWorld()
{
  using namespace geo;

  auto air  = G4Material::GetMaterial("G4_AIR");
  auto rock = G4Material::GetMaterial("Rock");

  // Three volumes, nested: World (air) > Rock > Hall (air), and the detectors go in
  // the Hall.
  //
  // Rock is the HALL'S OWN SHAPE GROWN OUTWARD by kRockThickness, and Hall sits
  // inside it as a daughter. Geant4 then leaves rock exactly where the daughter is
  // not, which is a shell of uniform thickness over the dome, under the floor and
  // around the pit walls alike -- with no subtraction anywhere. The cavern and the
  // pit used to be two separate air volumes with the rock built as
  // World - Cavern - Pit; that needed two subtractions, made the pit's mouth part
  // of the cavern's surface even though there is no rock behind it, and left the
  // pit's own coordinate frame for the floor modules to be placed in.
  const G4double t = hall::kRockThickness;
  const G4double wide = 2. * hall::kWorldRadius;

  auto solidWorld = new G4Orb("World", hall::kWorldRadius);
  auto logicWorld = new G4LogicalVolume(solidWorld, air, "World");
  auto physWorld  = new G4PVPlacement(nullptr, G4ThreeVector(), logicWorld, "World",
                                      nullptr, false, 0, true);

  // ---- the rock: the hall grown by t ----
  //
  // The dome part is clipped at z = -t rather than at the floor, which is what puts
  // t of rock under the floor. The pit part reaches t past the pit's own bottom and
  // t out on each side; it needs no growth upward, since above the floor the dome
  // part covers it and the Hall daughter empties it again.
  auto rockDomeOrb = new G4Orb("Rock_dome_orb", hall::kHallRadius + t);
  auto rockDomeBox = new G4Box("Rock_dome_box", wide, wide, hall::kHallRadius + t);
  auto rockDome = new G4IntersectionSolid(
      "Rock_dome", rockDomeOrb, rockDomeBox,
      nullptr, G4ThreeVector(0., 0., hall::kFloorZ - t + hall::kHallRadius + t));

  auto rockPitBox = new G4Box("Rock_pit", hall::pit::kHalfX + t, hall::pit::kHalfY + t,
                              0.5 * (hall::pit::kDepth + t));
  const G4ThreeVector rockPitCentre(0., 0., hall::pit::kTopZ - 0.5 * (hall::pit::kDepth + t));

  auto solidRock = new G4UnionSolid("Rock", rockDome, rockPitBox, nullptr, rockPitCentre);
  auto logicRock = new G4LogicalVolume(solidRock, rock, "Rock");
  new G4PVPlacement(nullptr, G4ThreeVector(), logicRock, "Rock", logicWorld, false, 0, true);

  // ---- the hall: the air, dome and pit as one volume ----
  //
  // Placed at the origin, so hall coordinates ARE world coordinates and nothing
  // placed in it needs a frame correction.
  auto hallOrb = new G4Orb("Hall_orb", hall::kHallRadius);
  auto hallBox = new G4Box("Hall_box", wide, wide, hall::kHallRadius);
  auto hallDome =
      new G4IntersectionSolid("Hall_dome", hallOrb, hallBox, nullptr,
                              G4ThreeVector(0., 0., hall::kFloorZ + hall::kHallRadius));

  auto hallPitBox = new G4Box("Hall_pit", hall::pit::kHalfX, hall::pit::kHalfY,
                              0.5 * hall::pit::kDepth);
  const G4ThreeVector hallPitCentre(0., 0., 0.5 * (hall::pit::kTopZ + hall::pit::kBottomZ));

  auto solidHall = new G4UnionSolid("Hall", hallDome, hallPitBox, nullptr, hallPitCentre);
  auto logicHall = new G4LogicalVolume(solidHall, air, "Hall");
  new G4PVPlacement(nullptr, G4ThreeVector(), logicHall, "Hall", logicRock, false, 0, true);

  logicWorld->SetVisAttributes(G4VisAttributes::GetInvisible());
  logicHall->SetVisAttributes(G4VisAttributes::GetInvisible());
  auto vaRock = new G4VisAttributes(G4Colour(0.45, 0.38, 0.30, 0.25));
  logicRock->SetVisAttributes(vaRock);

  fHallVolume = logicHall;

  return physWorld;
}



// WCMD -- the water Cherenkov muon detector: a steel tank of water with a dry
// inner room, a door on the +Y face, and 48 PMTs looking down from the ceiling.
//
// bottomZ is the height of the tank's underside in the world -- the surface it
// stands on -- with x and y on the hall axis. Everything inside is built relative
// to the centre derived from it, so raising or lowering the tank needs no other
// change.
void DetectorConstruction::BuildWCMD(G4LogicalVolume * world, G4double bottomZ)
{
  auto air      = G4Material::GetMaterial("G4_AIR");
  auto steel    = G4Material::GetMaterial("G4_STAINLESS-STEEL");
  auto water    = G4Material::GetMaterial("G4_WATER");
  auto vacuum   = G4Material::GetMaterial("G4_Galactic");
  auto glass    = G4Material::GetMaterial("G4_Pyrex_Glass");
  auto aluminum = G4Material::GetMaterial("G4_Al");

  // ---- shared dimensions ----
  // Z is vertical.  Door on +Y face, at -X corner.
  // waterZHalf fixed; waterZOffset computed after PMT LV is built.
  const G4double waterZHalf = geo::wcmd::kWaterHalfZ;
  const G4double cavHH      = geo::wcmd::kCavHalfZ;
  const G4double tankHalfZ  = geo::wcmd::kTankHalfZ;
  const G4double pmtTopGap  = geo::wcmd::kPMTTopGap;

  // The tank stands on bottomZ, so its centre is one half-height above that.
  const G4double centreZ = bottomZ + tankHalfZ;

  // ---- shared primitive solids (reused across multiple volume definitions) ----
  auto sCav   = new G4Box("s_TankCav",   geo::wcmd::kCavHalfXY, geo::wcmd::kCavHalfXY, cavHH);
  auto sDoor  = new G4Box("s_DoorBox",    geo::wcmd::kDoorHalfX, geo::wcmd::kDoorHalfY, tankHalfZ);
  auto sRoomO = new G4Box("s_RoomOuter", geo::wcmd::kRoomOuterHalfXY, geo::wcmd::kRoomOuterHalfXY, geo::wcmd::kRoomOuterHalfZ);
  auto sRoomI = new G4Box("s_RoomInner", geo::wcmd::kRoomInnerHalfXY, geo::wcmd::kRoomInnerHalfXY, geo::wcmd::kRoomInnerHalfZ);

  // ---- key positions (in TankAir local frame unless noted) ----
  const G4ThreeVector tankPos(0., 0., centreZ);                          // tank centre, world
  const G4ThreeVector roomPosT(0., 0., geo::wcmd::kRoomZ);                     // room centre, TankAir
  const G4ThreeVector doorPosT(geo::wcmd::kDoorX, geo::wcmd::kDoorY, 0. * mm);     // door centre, TankAir
  const G4ThreeVector doorPosW(geo::wcmd::kDoorX, geo::wcmd::kDoorY, centreZ);     // door centre, world
  const G4ThreeVector doorPosRoom(geo::wcmd::kDoorX, geo::wcmd::kDoorY, -geo::wcmd::kRoomZ); // door rel. to room

  // PMT (x, y) positions come from the survey table.
  const auto & pmtXY = geo::wcmd::kPMTxy;

  // ================================================================
  // TankSteel : outer shell minus cavity and door cutout
  // ================================================================
  auto sOuter  = new G4Box("s_TankOuter", geo::wcmd::kTankHalfXY, geo::wcmd::kTankHalfXY, tankHalfZ);
  auto sShell1 = new G4SubtractionSolid("s_Shell1", sOuter, sCav);
  auto sShell  = new G4SubtractionSolid("s_Shell",  sShell1, sDoor, nullptr, doorPosT);
  auto lvShell = new G4LogicalVolume(sShell, steel, "TankSteel");
  new G4PVPlacement(nullptr, tankPos, lvShell, "TankSteel", world, false, 0, true);

  // ================================================================
  // DoorSteel : door outer shell minus inner cutout
  // (extends 8 mm through the tank wall → placed in World, not TankAir)
  // ================================================================
  auto sDoorI      = new G4Box("s_DoorInner",  geo::wcmd::kDoorInnerHalfX, geo::wcmd::kDoorInnerHalfY, geo::wcmd::kDoorInnerHalfZ);
  auto sDoorShell  = new G4SubtractionSolid("s_DoorShell", sDoor, sDoorI);
  auto lvDoorSteel = new G4LogicalVolume(sDoorShell, steel, "DoorSteel");
  new G4PVPlacement(nullptr, doorPosW, lvDoorSteel, "DoorSteel", world, false, 0, true);

  // ================================================================
  // TankAir : cavity minus the door, common mother for the internal volumes
  //
  // The door spans the full 8 mm wall thickness, so it reaches past the cavity
  // and has to live in World. TankSteel, TankWater and RoomSteel all subtract it;
  // the cavity has to as well, or DoorSteel overlaps it.
  // ================================================================
  auto sCavFinal = new G4SubtractionSolid("s_TankCavFinal", sCav, sDoor, nullptr, doorPosT);
  auto lvTankAir = new G4LogicalVolume(sCavFinal, air, "TankAir");
  new G4PVPlacement(nullptr, tankPos, lvTankAir, "TankAir", world, false, 0, true);

  // ================================================================
  // PMT logical volume
  // Must be built before TankWater because waterZOffset depends on PMT dimensions.
  // ================================================================
  auto sdMan = G4SDManager::GetSDMpointer();
  auto pmtSD = new PMTSD("/WCMD/PMTSD");
  sdMan->AddNewDetector(pmtSD);

  // ExteriorMat = water so the photocathode boundary is water→glass (no air gap).
  auto pmtLog  = new PMT10inchLogicalVolume("PMT", water, glass, fPhotocathodeSurface,
                                            vacuum, aluminum, nullptr, pmtSD);
  auto pmtTubs = (G4Tubs *)pmtLog->GetSolid();
  const G4double pmtHH = pmtTubs->GetZHalfLength(); // 165 mm
  const G4double pmtR  = pmtTubs->GetOuterRadius();  // 126.5 mm
  const G4double zEq   = pmtLog->GetZEquator();      // ≈ 68.3 mm

  // After rotateX(180°): PMT local +Z → world -Z; photocathode apex sits
  // (pmtHH − zEq) ≈ 96.7 mm below the water surface.
  const G4double holeHH = pmtHH - zEq;    // hole depth = photocathode depth
  const G4double holeR  = pmtR + 0.1 * mm; // 0.1 mm tolerance

  // waterZOffset: places the water top face 1 mm below the tank ceiling.
  // Condition: waterZOffset + waterZHalf + zEq + pmtHH = cavHH − pmtTopGap
  const G4double waterZOffset = cavHH - pmtTopGap - waterZHalf - zEq - pmtHH;

  const G4ThreeVector roomPosWaterT = roomPosT - G4ThreeVector(0, 0, waterZOffset);
  const G4ThreeVector doorPosWaterT = doorPosT - G4ThreeVector(0, 0, waterZOffset);

  fWaterRegionBounds = {geo::wcmd::kRoomOuterHalfXY, geo::wcmd::kRoomOuterHalfXY,
                        roomPosWaterT.z() + geo::wcmd::kRoomOuterHalfZ};

  // ================================================================
  // TankWater : water with room, door, and PMT holes subtracted
  // ================================================================
  auto pmtHoleTube = new G4Tubs("PMThole", 0., holeR, holeHH, 0., CLHEP::twopi);
  auto allHoles    = new G4MultiUnion("AllPMTHoles");
  G4RotationMatrix noRot;
  for (int i = 0; i < geo::wcmd::kNPMT; i++) {
    // Hole centres at the top face of the WaterCav solid (z = +waterZHalf)
    G4Transform3D tr(noRot, G4ThreeVector(pmtXY[i][0] * mm, pmtXY[i][1] * mm, waterZHalf));
    allHoles->AddNode(*pmtHoleTube, tr);
  }
  allHoles->Voxelize();

  auto sWaterCav   = new G4Box("s_WaterCav", geo::wcmd::kCavHalfXY, geo::wcmd::kCavHalfXY, waterZHalf);
  auto sWater1     = new G4SubtractionSolid("s_Water1",     sWaterCav, sRoomO,   nullptr, roomPosWaterT);
  auto sWater      = new G4SubtractionSolid("s_Water",      sWater1,   sDoor,    nullptr, doorPosWaterT);
  auto sWaterFinal = new G4SubtractionSolid("s_WaterFinal", sWater,    allHoles);
  auto lvWater     = new G4LogicalVolume(sWaterFinal, water, "TankWater");
  fScoringVolume   = lvWater;
  auto physWater   = new G4PVPlacement(nullptr, G4ThreeVector(0, 0, waterZOffset),
                                       lvWater, "TankWater", lvTankAir, false, 0, true);

  // ================================================================
  // DoorWater : water inside the door with PMT #47 hole subtracted
  // ================================================================
  const G4double dw_lx = pmtXY[47][0] * mm - doorPosT.x(); // −2328 − (−2327.5) = −0.5 mm
  const G4double dw_ly = pmtXY[47][1] * mm - doorPosT.y(); //  2850 −  2855     = −5.0 mm
  auto sDoorWaterS     = new G4Box("s_DoorWaterS", geo::wcmd::kDoorInnerHalfX, geo::wcmd::kDoorInnerHalfY, waterZHalf);
  auto doorPMTHole     = new G4Tubs("DoorPMThole", 0., holeR, holeHH, 0., CLHEP::twopi);
  auto sDoorWaterFinal = new G4SubtractionSolid("s_DoorWaterFinal", sDoorWaterS, doorPMTHole,
                                                nullptr, G4ThreeVector(dw_lx, dw_ly, waterZHalf));
  auto lvDoorWater = new G4LogicalVolume(sDoorWaterFinal, water, "DoorWater");
  // In World, not TankAir: the door region is carved out of the cavity, and this
  // water fills the hollow of the door shell.
  auto physDoorWater = new G4PVPlacement(
      nullptr, G4ThreeVector(doorPosT.x(), doorPosT.y(), tankPos.z() + waterZOffset),
      lvDoorWater, "DoorWater", world, false, 0, true);

  // ================================================================
  // RoomSteel : liner shell around the inner room (with door cutout)
  // ================================================================
  auto sRoomShell1 = new G4SubtractionSolid("s_RoomShell1", sRoomO, sRoomI);
  auto sRoomShell  = new G4SubtractionSolid("s_RoomShell",  sRoomShell1, sDoor, nullptr, doorPosRoom);
  auto lvRoomSteel = new G4LogicalVolume(sRoomShell, steel, "RoomSteel");
  new G4PVPlacement(nullptr, roomPosT, lvRoomSteel, "RoomSteel", lvTankAir, false, 0, true);

  // ================================================================
  // RoomAir : interior of the inner room
  // ================================================================
  auto lvRoomAir = new G4LogicalVolume(sRoomI, air, "RoomAir");
  fRoomAirVolume = lvRoomAir; // the dry space the shielding would stand in
  new G4PVPlacement(nullptr, roomPosT, lvRoomAir, "RoomAir", lvTankAir, false, 0, true);

  // ================================================================
  // PMT placements inside TankAir (×48)
  // After rotateX(180°): PMT local +Z → world −Z (photocathode faces down).
  // PMT centre placed so its equator lands at the water surface.
  // ================================================================
  auto rotPMT = new G4RotationMatrix();
  rotPMT->rotateX(180. * deg);
  const G4double pmtCentreZ = waterZOffset + waterZHalf + zEq; // in TankAir local

  // PMT 47 is the door PMT: it sits inside the door, so it is placed in World
  // alongside the door and its transparent border is with DoorWater. Pairing it
  // with TankWater instead would leave the DoorWater Tyvek skin in force at its
  // photocathode, which makes it reflective -- i.e. blind.
  const int kDoorPMT = geo::wcmd::kDoorPMT;

  char PMTname[64];
  for (int i = 0; i < geo::wcmd::kNPMT; i++) {
    sprintf(PMTname, "PMTPhys%d", i);

    const bool inDoor = (i == kDoorPMT);
    G4LogicalVolume * mother = inDoor ? world : lvTankAir;
    const G4double zPos = inDoor ? tankPos.z() + pmtCentreZ : pmtCentreZ;

    auto physPMT = new G4PVPlacement(
        rotPMT, G4ThreeVector(pmtXY[i][0] * mm, pmtXY[i][1] * mm, zPos),
        pmtLog, PMTname, mother, false, i, true);

    new G4LogicalBorderSurface("WaterPMT_" + std::to_string(i),
                               inDoor ? physDoorWater : physWater, physPMT,
                               fWaterPMTSurface);
  }

  // ================================================================
  // Optical surfaces
  // ================================================================
  new G4LogicalSkinSurface("TyvekSkinMain", lvWater,    fTyvekSurface);
  new G4LogicalSkinSurface("TyvekSkinDoor", lvDoorWater, fTyvekSurface);
  new G4LogicalSkinSurface("TyvekSkinAir",  lvTankAir,  fTyvekSurface);

  // ================================================================
  // Visualisation
  // ================================================================
  auto vaSteel = new G4VisAttributes(G4Colour(0.68, 0.72, 0.77, 0.30));
  auto vaWater = new G4VisAttributes(G4Colour(0.18, 0.62, 0.84, 0.35));
  auto vaDoorS = new G4VisAttributes(G4Colour(0.79, 0.59, 0.18, 0.45));
  auto vaRoom  = new G4VisAttributes(G4Colour(0.20, 0.27, 0.35, 0.20));
  auto vaAir   = new G4VisAttributes(G4Colour(0.90, 0.90, 0.90, 0.05));
  vaSteel->SetForceSolid(true);
  vaWater->SetForceSolid(true);
  vaDoorS->SetForceSolid(true);
  lvTankAir->SetVisAttributes(vaAir);
  lvShell->SetVisAttributes(vaSteel);
  lvWater->SetVisAttributes(vaWater);
  lvRoomSteel->SetVisAttributes(vaSteel);
  lvDoorSteel->SetVisAttributes(vaDoorS);
  lvDoorWater->SetVisAttributes(vaWater);
  lvRoomAir->SetVisAttributes(vaRoom);

}


// One PSMD module: a 1680 x 310 x 60 mm aluminium box with a 1 mm wall, holding
// two 1670 x 303 x 15.5 mm plastic scintillator slabs. Each slab is set 1 mm in
// from the wall it faces, leaving a 25 mm air gap between the two.
//
// The envelope is air and exactly the size of the aluminium outer box, so the
// module occupies no more space than the box itself: whoever places it only has
// to keep clear of its own outline.
G4LogicalVolume * DetectorConstruction::BuildPSMDModule()
{
  auto air      = G4Material::GetMaterial("G4_AIR");
  auto aluminum = G4Material::GetMaterial("G4_Al");
  auto scint    = G4Material::GetMaterial("G4_PLASTIC_SC_VINYLTOLUENE");

  const G4double alHX = geo::psmd::kAlHalfX, alHY = geo::psmd::kAlHalfY,
                 alHZ = geo::psmd::kAlHalfZ;
  const G4double wall = geo::psmd::kWall;
  const G4double gap  = geo::psmd::kGap;   // slab to inner wall face

  const G4double scHX = geo::psmd::kScintHalfX, scHY = geo::psmd::kScintHalfY,
                 scHZ = geo::psmd::kScintHalfZ;

  // Slab centre offset: inner face of the wall, less the gap, less its own half
  // thickness → 30 − 1 − 1 − 7.75 = 20.25 mm either side of the module centre.
  const G4double scZ = alHZ - wall - gap - scHZ;

  // ---- air envelope: the module's mother volume ----
  auto sEnv  = new G4Box("s_PSMDModule", alHX, alHY, alHZ);
  auto lvEnv = new G4LogicalVolume(sEnv, air, "PSMDModule");
  lvEnv->SetVisAttributes(G4VisAttributes::GetInvisible());

  // ---- aluminium shell ----
  auto sAlOuter = new G4Box("s_PSMDAlOuter", alHX, alHY, alHZ);
  auto sAlInner = new G4Box("s_PSMDAlInner", alHX - wall, alHY - wall, alHZ - wall);
  auto sAlShell = new G4SubtractionSolid("s_PSMDAlShell", sAlOuter, sAlInner);
  auto lvAlShell = new G4LogicalVolume(sAlShell, aluminum, "PSMDAlShell");
  new G4PVPlacement(nullptr, G4ThreeVector(), lvAlShell, "PSMDAlShell", lvEnv, false, 0, true);

  // ---- the two scintillator slabs ----
  // One logical volume placed twice; the copy number tells them apart.
  auto sScint  = new G4Box("s_PSMDScint", scHX, scHY, scHZ);
  auto lvScint = new G4LogicalVolume(sScint, scint, "PSMDScint");

  // No sensitive detector here. The slab's energy is summed in RootManager,
  // which already receives every step together with the slab MakeDetectorIDTagger
  // puts it in -- so an SD would only be a second path to the same numbers.
  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., +scZ), lvScint, "PSMDScintTop",
                    lvEnv, false, 0, true);
  new G4PVPlacement(nullptr, G4ThreeVector(0., 0., -scZ), lvScint, "PSMDScintBottom",
                    lvEnv, false, 1, true);

  auto vaAl    = new G4VisAttributes(G4Colour(0.68, 0.72, 0.77, 0.30));
  auto vaScint = new G4VisAttributes(G4Colour(0.35, 0.80, 0.45, 0.45));
  vaScint->SetForceSolid(true);
  lvAlShell->SetVisAttributes(vaAl);
  lvScint->SetVisAttributes(vaScint);

  return lvEnv;
}


// Survey table: where each of the 130 modules sits in the world. A module's row
// index here IS its detector ID, so reordering this table renumbers the detector.
//
// PROVISIONAL. The module itself is measured, but its layout is not: this is a
// single flat plane of 5 x 26 modules above the water tank, chosen only so that
// all 130 exist, carry IDs and can be exercised end to end. The numbers below are
// the ones to replace with survey data -- nothing else has to change.
std::vector<DetectorConstruction::PSMDPlacement> DetectorConstruction::PSMDLayout()
{
  using namespace geo;

  const G4double halfLong  = psmd::kAlHalfX;  // 840 -- half the 1680 mm side
  const G4double halfShort = psmd::kAlHalfY;  // 155 -- half the 310 mm side
  const G4double halfThick = psmd::kAlHalfZ;  // 30

  // How far a module's centre sits from the shielding's axis: past the HDPE face,
  // across the gap, then half its own thickness.
  const G4double standoff = shield::kCavityHalfX + shield::kHDPE + shield::kBoratedRubber
                            + shield::kLead + shield::kInner + psmd::wall::kGap + halfThick;

  // Where the groups reach along their wall. The x-face groups stop at the inner
  // surface of the y-face groups; the y-face groups run on past, to the outer
  // surface of the x-face ones, which is what closes the corner.
  const G4double innerSurface = standoff - halfThick;
  const G4double outerSurface = standoff + halfThick;

  const int nUp = psmd::wall::kModulesPerGroup;
  const G4double stackBottom = hall::kFloorZ; // groups start where the HDPE does

  std::vector<PSMDPlacement> out;
  out.reserve(psmd::wall::kWallModules);

  // A face: its outward normal, the direction its groups run along, how far they
  // reach, and the roll that lays the long side horizontal.
  //
  // The roll differs between the two pairs of faces and this is measured, not
  // guessed: swinging the module's local +z onto the normal turns about
  // (z_hat x normal), which for a y-face leaves the long axis already along x, but
  // for an x-face carries it into the vertical. Hence 0 there and 90 degrees here.
  // firstId[0] is the group on the negative side of `along`, firstId[1] the positive
  // one, and a group's ids run upward with its stack -- lowest module, lowest id.
  // Taken from the survey drawing, which numbers the groups anticlockwise from the
  // +y face and leaves gaps for the modules that are not installed.
  struct Face {
    G4ThreeVector normal;
    G4ThreeVector along;
    G4double reach;
    G4double roll;
    int firstId[2];
  };
  const Face faces[] = {
      {{0., 1., 0.}, {1., 0., 0.}, outerSurface, 0., {1, 102}},   // Grp 1, Grp 8
      {{0., -1., 0.}, {1., 0., 0.}, outerSurface, 0., {37, 66}},  // Grp 4, Grp 5
      {{1., 0., 0.}, {0., 1., 0.}, innerSurface, 90. * deg, {78, 90}},  // Grp 6, Grp 7
      {{-1., 0., 0.}, {0., 1., 0.}, innerSurface, 90. * deg, {25, 13}}, // Grp 3, Grp 2
  };

  for (const auto & face : faces) {
    // Two groups, each one module long, pushed out to either end of the reach. What
    // is left in the middle is the gap.
    const G4double groupCentre = face.reach - halfLong;

    for (int g = 0; g < psmd::wall::kGroupsPerFace; g++) {
      const G4double sign = (g == 0) ? -1. : +1.;
      for (int k = 0; k < nUp; k++) {
        const G4ThreeVector centre =
            G4ThreeVector(shield::kX, shield::kY, 0.) + standoff * face.normal
            + (sign * groupCentre) * face.along
            + G4ThreeVector(0., 0., stackBottom + halfShort + 2. * halfShort * k);
        out.push_back({face.firstId[g] + k, centre, face.normal, face.roll});
      }
    }
  }

  return out;
}


// PSMD -- the plastic scintillator muon detector: geo::psmd::kModules identical modules
// placed straight into the world.
//
// Each placement carries its layout-table index twice over: as its copy number,
// which is what MakeDetectorIDTagger reads back to identify the slab that was
// hit, and in its name, PSMDModule<i>, so that a generator can aim at one module
// ("involume PSMDModule7") or at all of them ("multivolume PSMDModule"). The
// copy number alone would not do for the latter, since the position generators
// resolve volumes by name.
// The modules lying in the pit under the shielding.
//
// Hall coordinates, like everything else: the pit is part of the Hall volume and
// the Hall sits at the origin, so there is no frame to correct for. This used to
// take a motherZ and subtract it, back when the pit was its own volume placed off
// the origin -- a correction that silently failed once and put the modules a metre
// out.
std::vector<DetectorConstruction::PSMDPlacement>
DetectorConstruction::PSMDFloorLayout()
{
  using namespace geo;

  const G4double halfLong  = psmd::kAlHalfX;
  const G4double halfShort = psmd::kAlHalfY;
  const G4double halfThick = psmd::kAlHalfZ;

  std::vector<PSMDPlacement> out;
  out.reserve(psmd::floorLayer::kFloorModules);

  // ---- the floor layer, lying in the pit ----
  //
  // Eleven modules across in x, two deep in y, long side along y. The top face sits
  // kGapBelowShield under the shielding's underside, measured face to face as the
  // wall gap is.
  //
  // Upper slab DOWN here, away from the shielding -- which is the same sense as the
  // wall groups, whose upper slab also faces outward. Measured with a downward muon
  // through a floor module: it meets PSMDScintBottom first, then PSMDScintTop.
  //
  // The normal being antiparallel to +z is the degenerate case for the axis/angle
  // construction in BuildPSMD, which handles it as rotateX(180 deg). That preserves
  // the module's local x, so the id-to-x mapping below is untouched and Det 55 and
  // Det 120 still sit at -x. It does reverse local y, so the drawing's readout ends
  // (ch3/ch1 at +y) swap -- which changes nothing today, no readout being simulated,
  // but matters when one is added.
  //
  // Both rows number left to right: Det 55 and Det 120 sit at -x, Det 65 and Det 130
  // at +x. From the top-view drawing -- not the anticlockwise walk the wall groups
  // follow, which would have run the +y row the other way.
  const G4double topZ = hall::kFloorZ - psmd::floorLayer::kGapBelowShield;
  const G4double centreZ = topZ - halfThick;
  const G4double pitchX = 2. * halfShort;

  struct Row {
    G4double y;
    int firstId;
    int step; // +1 as x rises, -1 as x falls
  };
  const Row rows[] = {{-halfLong, 55, +1}, {+halfLong, 120, +1}};

  for (const auto & row : rows) {
    for (int i = 0; i < psmd::floorLayer::kColumns; i++) {
      const G4double x = (i - 0.5 * (psmd::floorLayer::kColumns - 1)) * pitchX;
      const int id = row.firstId + row.step * i;
      out.push_back({id, G4ThreeVector(shield::kX + x, shield::kY + row.y, centreZ),
                     G4ThreeVector(0., 0., -1.), 90. * deg});
    }
  }


  return out;
}


void DetectorConstruction::BuildPSMD(G4LogicalVolume * world,
                                     const std::vector<PSMDPlacement> & placements)
{
  if (placements.empty()) {
    G4cout << "PSMD: no module layout defined -- 0 of " << geo::psmd::kModules
           << " modules placed (see DetectorConstruction::PSMDLayout)." << G4endl;
    return;
  }
  // The numbering, not the count, is what has to be right: some module numbers are
  // deliberately not installed, so a short table is expected. What must not happen
  // is two modules claiming one number, or a number outside the detector's range.
  for (const auto & p : placements) {
    if (p.id < 1 || p.id > geo::psmd::kModules) {
      G4Exception("DetectorConstruction::BuildPSMD", "GEO001", FatalException,
                  ("module id " + std::to_string(p.id) + " is outside 1.."
                   + std::to_string(geo::psmd::kModules)).c_str());
    }
    if (!fPSMDIds.insert(p.id).second) {
      G4Exception("DetectorConstruction::BuildPSMD", "GEO005", FatalException,
                  ("two modules share id " + std::to_string(p.id)).c_str());
    }
  }

  // Built once: 130 placements of one logical volume, not 130 copies of it.
  G4LogicalVolume * lvModule = BuildPSMDModule();

  for (std::size_t i = 0; i < placements.size(); i++) {
    const PSMDPlacement & p = placements[i];

    // Rotation that takes the module's local +z onto its normal, then rolls it
    // about that. Same construction as the H-beam members, and the same caveat:
    // G4PVPlacement's matrix turns the mother frame into the daughter's, so the
    // matrix handed over is the inverse of the module's own rotation. Getting that
    // backwards mirrors every placement that is not symmetric under it.
    //
    // A fresh matrix per placement: G4PVPlacement keeps the pointer rather than
    // copying it, so placements must not share one mutable matrix.
    G4RotationMatrix rotObj;
    const G4ThreeVector normal = p.normal.unit();
    const G4ThreeVector zAxis(0., 0., 1.);
    const G4double cosAngle = zAxis.dot(normal);
    if (cosAngle < 1. - 1e-12) {
      if (cosAngle < -1. + 1e-12) { rotObj.rotateX(180. * deg); } // upside down
      else {
        const G4ThreeVector turn = zAxis.cross(normal).unit();
        rotObj.rotate(std::acos(cosAngle), turn);
      }
    }
    if (p.roll != 0.) rotObj.rotate(p.roll, normal);

    // nullptr for an unrotated module, so a flat layout places exactly as it did
    // before this became a direction rather than an angle.
    G4RotationMatrix * rot =
        rotObj.isIdentity() ? nullptr : new G4RotationMatrix(rotObj.inverse());

    new G4PVPlacement(rot, p.centre, lvModule, "PSMDModule" + std::to_string(p.id), world,
                      false, p.id, true);
  }
}


// Which PSMD slab a step deposited its energy in.
//
// The slab is one logical volume placed twice inside a module envelope that is
// itself placed 130 times, so all 260 slabs share the two names PSMDScintTop and
// PSMDScintBottom. The identity is in the copy numbers along the touchable
// history: depth 0 is the slab (0 top, 1 bottom), depth 1 the module envelope,
// whose copy number is its row in the layout table.
//
// Read off the PRE-step volume, because that is where the step's energy deposit
// happened. Note MCStep's other fields describe the step's end point -- including
// fVolumeName, which for a step ending on a boundary names the volume being
// entered. So fDetectorID and fVolumeName can disagree on such a step, and it is
// fDetectorID that goes with fEnergyDeposit.
// One H-beam member: the exact H section, extruded along the member's local z.
//
// In the member's own frame the section lies in x-y -- width along x, depth along
// y -- and the length runs along z. The H is a full box with the two channels
// between the flanges taken out, which is exact and needs no fillet fudging: two
// subtractions rather than a union of three plates that would share faces.
G4LogicalVolume * DetectorConstruction::MakeHBeam(const G4String & name,
                                                 const geo::frame::HBeamSection & section, G4double length)
{
  auto steel = G4Material::GetMaterial("G4_Fe");

  const G4double halfW = 0.5 * section.B;
  const G4double halfD = 0.5 * section.H;
  const G4double halfL = 0.5 * length;

  // Each channel spans x from web/2 out to width/2, and y between the flanges.
  const G4double chanHalfW = 0.5 * (halfW - 0.5 * section.t1);
  const G4double chanHalfD = halfD - section.t2;
  const G4double chanX = 0.5 * section.t1 + chanHalfW;

  auto solid = new G4Box(("s_" + name + "_box").c_str(), halfW, halfD, halfL);
  auto chan = new G4Box(("s_" + name + "_chan").c_str(), chanHalfW, chanHalfD, halfL + 1. * mm);

  auto cut1 = new G4SubtractionSolid(("s_" + name + "_c1").c_str(), solid, chan, nullptr,
                                     G4ThreeVector(+chanX, 0., 0.));
  auto cut2 = new G4SubtractionSolid(("s_" + name).c_str(), cut1, chan, nullptr,
                                     G4ThreeVector(-chanX, 0., 0.));

  return new G4LogicalVolume(cut2, steel, name);
}


// The steel frame. Members are placed straight into the mother, each named
// HBeam<i> after its row in the layout table.
void DetectorConstruction::BuildHBeamFrame(G4LogicalVolume * mother,
                                           const geo::frame::HBeamSection & section,
                                           const std::vector<HBeamMember> & members)
{
  if (!mother || members.empty()) return;

  auto vaSteel = new G4VisAttributes(G4Colour(0.30, 0.35, 0.45, 0.55));
  vaSteel->SetForceSolid(true);

  G4double totalLength = 0.;

  for (std::size_t i = 0; i < members.size(); i++) {
    const HBeamMember & m = members[i];
    const G4String name = "HBeam" + std::to_string(i);

    G4LogicalVolume * lv = MakeHBeam(name, section, m.length);
    lv->SetVisAttributes(vaSteel);

    // Rotation that takes the member's local z onto its axis, then rolls the
    // section about it. G4PVPlacement wants the inverse of the object's own
    // rotation -- it rotates the mother's frame into the daughter's -- so the
    // matrix built here is inverted before it is handed over.
    G4RotationMatrix rotObj;
    const G4ThreeVector axis = m.axis.unit();
    const G4ThreeVector zAxis(0., 0., 1.);
    const G4double cosAngle = zAxis.dot(axis);
    if (cosAngle < 1. - 1e-12) {
      if (cosAngle < -1. + 1e-12) { rotObj.rotateX(180. * deg); } // antiparallel
      else {
        const G4ThreeVector turn = zAxis.cross(axis).unit();
        rotObj.rotate(std::acos(cosAngle), turn);
      }
    }
    if (m.roll != 0.) rotObj.rotate(m.roll, axis);

    auto * rot = new G4RotationMatrix(rotObj.inverse());
    new G4PVPlacement(rot, m.centre, lv, name, mother, false, (G4int)i, true);

    totalLength += m.length;
  }

  // Section area from the plates: two flanges plus the web between them.
  const G4double area = 2. * section.B * section.t2
                        + section.t1 * (section.H - 2. * section.t2);
  const G4double linearMass = area * (7.874 * g / cm3);

  G4cout << "H-beam frame: " << members.size() << " members, " << totalLength / m << " m of "
         << "H-" << section.H / mm << "x" << section.B / mm << "x" << section.t1 / mm
         << "x" << section.t2 / mm << " (" << linearMass / (kg / m) << " kg/m), "
         << totalLength * linearMass / kg << " kg of steel." << G4endl;
}


// The frame layout. A member's row index here is its copy number.
//
// PROVISIONAL. Eight perimeter columns -- corners and mid-sides -- on a footprint
// that matches the water tank they carry, with a ring of girders at the top. No
// interior column and no cross girder: the shielding stands in the middle and the
// space above it is where the tank's load would have to be carried across, which
// is a structural question this table cannot invent.
std::vector<DetectorConstruction::HBeamMember> DetectorConstruction::HBeamFrameLayout()
{
  const G4double halfSpan = geo::frame::kHalfSpan;
  const G4double girderDepth = geo::frame::kSection.H; // girders hang below the top
  const G4double girderZ = geo::frame::kTopZ - 0.5 * girderDepth;
  const G4double colTop = geo::frame::kTopZ - girderDepth;
  const G4double colBottom = geo::hall::kFloorZ;
  const G4double colLength = colTop - colBottom;

  std::vector<HBeamMember> out;

  // ---- columns, standing on the cavern floor ----
  const G4double xs[] = {-halfSpan, 0., halfSpan};
  const G4double ys[] = {-halfSpan, 0., halfSpan};
  for (G4double x : xs) {
    for (G4double y : ys) {
      if (x == 0. && y == 0.) continue; // no column through the middle of the room
      out.push_back({G4ThreeVector(x, y, colBottom + 0.5 * colLength), colLength,
                     G4ThreeVector(0., 0., 1.), 0.});
    }
  }

  // ---- top girders, spanning between the columns ----
  // Each side of the square in two spans, so members meet at the mid-side columns
  // rather than running through them.
  //
  // The two directions cannot both own the corners. The x girders run through, and
  // the y girders are framed into their webs -- shortened by half a section width
  // at the corner end and shifted in to match, which is how the steel is actually
  // connected. Without it the four corners are four overlapping volumes.
  const G4double span = halfSpan;
  const G4double mid = 0.5 * halfSpan;
  const G4double trim = 0.5 * geo::frame::kSection.B;

  for (G4double y : {-halfSpan, halfSpan}) {
    for (G4double sx : {-1., 1.})
      out.push_back({G4ThreeVector(sx * mid, y, girderZ), span, G4ThreeVector(1., 0., 0.), 0.});
  }
  for (G4double x : {-halfSpan, halfSpan}) {
    for (G4double sy : {-1., 1.})
      out.push_back({G4ThreeVector(x, sy * (mid - 0.5 * trim), girderZ), span - trim,
                     G4ThreeVector(0., 1., 0.), 0.});
  }

  return out;
}


// The passive shielding: four nested layers around a box that is open at the top,
// outermost first -- 700 mm HDPE, 10 mm borated rubber, 200 mm lead, 50 mm of lead
// or copper -- around a 1400 x 1400 x 2905 mm cavity.
//
// Open at the top means every layer's top face is at the same height and only the
// bottoms are staggered, so a layer is not a symmetric shell: it is a cup. Each
// one is its own box minus the box of the layer inside it.
//
// bottomCentre is the centre of the outermost layer's underside, in the MOTHER's
// frame -- for a mother that is not the world, that is not a world coordinate.
void DetectorConstruction::BuildShielding(G4LogicalVolume * mother,
                                          const G4ThreeVector & bottomCentre,
                                          ShieldInner inner)
{
  if (!mother) {
    G4Exception("DetectorConstruction::BuildShielding", "GEO002", JustWarning,
                "No mother volume -- shielding not built.");
    return;
  }

  const G4double bottomZ = bottomCentre.z();

  // ---- the cavity the shielding encloses ----
  const G4double cavHX = geo::shield::kCavityHalfX;    // 1400 mm across
  const G4double cavHY = geo::shield::kCavityHalfY;
  const G4double cavH  = geo::shield::kCavityH;   // full height

  // ---- layers, outermost first ----
  struct Layer {
    const char * name;
    G4double thickness;
    G4Material * material;
  };
  const Layer layers[] = {
      {"ShieldHDPE",          geo::shield::kHDPE, G4Material::GetMaterial("G4_POLYETHYLENE")},
      {"ShieldBoratedRubber",  geo::shield::kBoratedRubber, G4Material::GetMaterial("BoratedRubber")},
      {"ShieldLead",          geo::shield::kLead, G4Material::GetMaterial("G4_Pb")},
      {(inner == ShieldInner::Copper) ? "ShieldCopper" : "ShieldLeadInner", geo::shield::kInner,
       (inner == ShieldInner::Copper) ? G4Material::GetMaterial("G4_Cu")
                                      : G4Material::GetMaterial("G4_Pb")},
  };
  const int nLayers = (int)(sizeof(layers) / sizeof(layers[0]));

  // Half-widths and full heights of every boundary box, from the outermost layer
  // in to the cavity. Each step inward loses the layer's thickness from all four
  // sides and from the bottom -- never from the top, which stays open.
  G4double wallTotal = 0.;
  for (const auto & l : layers)
    wallTotal += l.thickness;

  // Every box shares this top face; that is what makes the structure open.
  const G4double topZ = bottomZ + wallTotal + cavH;

  auto halfX = [&](int i) {                       // i == nLayers means the cavity
    G4double t = 0.;
    for (int k = 0; k < i; k++)
      t += layers[k].thickness;
    return cavHX + (wallTotal - t);
  };
  auto halfY = [&](int i) {
    G4double t = 0.;
    for (int k = 0; k < i; k++)
      t += layers[k].thickness;
    return cavHY + (wallTotal - t);
  };
  auto bottomOf = [&](int i) {
    G4double t = 0.;
    for (int k = 0; k < i; k++)
      t += layers[k].thickness;
    return bottomZ + t;
  };

  // 1 mm of overshoot on the cutting box, so the open top is a clean face rather
  // than two solids ending on exactly the same plane.
  const G4double kOvershoot = geo::shield::kCutOvershoot;

  G4LogicalVolume * lvLayer[8] = {};

  for (int i = 0; i < nLayers; i++) {
    const G4double zLo = bottomOf(i);
    const G4double hz = 0.5 * (topZ - zLo);
    const G4double cz = 0.5 * (topZ + zLo);

    auto outer = new G4Box((std::string("s_") + layers[i].name + "_outer").c_str(),
                           halfX(i), halfY(i), hz);

    // The next boundary in, grown upward so it cuts through the top face.
    const G4double zLoIn = bottomOf(i + 1);
    const G4double hzIn = 0.5 * (topZ - zLoIn) + kOvershoot;
    const G4double czIn = 0.5 * (topZ + zLoIn) + kOvershoot;
    auto cutter = new G4Box((std::string("s_") + layers[i].name + "_cut").c_str(),
                            halfX(i + 1), halfY(i + 1), hzIn);

    auto solid = new G4SubtractionSolid(
        (std::string("s_") + layers[i].name).c_str(), outer, cutter, nullptr,
        G4ThreeVector(0., 0., czIn - cz));

    lvLayer[i] = new G4LogicalVolume(solid, layers[i].material, layers[i].name);
    new G4PVPlacement(nullptr, G4ThreeVector(bottomCentre.x(), bottomCentre.y(), cz), lvLayer[i],
                      layers[i].name, mother, false, 0, true);
  }

  // ---- the cavity itself ----
  // Filled with the mother's own material, so it changes no physics; it exists to
  // be a mother for whatever goes inside and a target for "involume ShieldCavity".
  auto sCav = new G4Box("s_ShieldCavity", cavHX, cavHY, 0.5 * cavH);
  auto lvCav = new G4LogicalVolume(sCav, mother->GetMaterial(), "ShieldCavity");
  new G4PVPlacement(nullptr,
                    G4ThreeVector(bottomCentre.x(), bottomCentre.y(), topZ - 0.5 * cavH), lvCav,
                    "ShieldCavity", mother, false, 0, true);

  // ---- visualisation ----
  auto vaHDPE   = new G4VisAttributes(G4Colour(0.90, 0.90, 0.80, 0.25));
  auto vaRubber = new G4VisAttributes(G4Colour(0.20, 0.20, 0.20, 0.60));
  auto vaLead   = new G4VisAttributes(G4Colour(0.35, 0.35, 0.42, 0.55));
  auto vaInner  = new G4VisAttributes((inner == ShieldInner::Copper)
                                          ? G4Colour(0.72, 0.45, 0.20, 0.70)
                                          : G4Colour(0.45, 0.45, 0.52, 0.70));
  for (auto * va : {vaHDPE, vaRubber, vaLead, vaInner})
    va->SetForceSolid(true);
  if (lvLayer[0]) lvLayer[0]->SetVisAttributes(vaHDPE);
  if (lvLayer[1]) lvLayer[1]->SetVisAttributes(vaRubber);
  if (lvLayer[2]) lvLayer[2]->SetVisAttributes(vaLead);
  if (lvLayer[3]) lvLayer[3]->SetVisAttributes(vaInner);
  lvCav->SetVisAttributes(G4VisAttributes::GetInvisible());

  G4cout << "Shielding: outer " << 2. * halfX(0) / mm << " x " << 2. * halfY(0) / mm << " x "
         << (topZ - bottomZ) / mm << " mm, cavity " << 2. * cavHX / mm << " x " << 2. * cavHY / mm
         << " x " << cavH / mm << " mm, standing at (" << bottomCentre.x() / mm << ", "
         << bottomCentre.y() / mm << ", " << bottomZ / mm << ") mm, open top at z = " << topZ / mm
         << " mm in its mother's frame." << G4endl;
}


std::function<G4int(const G4Step *)> DetectorConstruction::MakeDetectorIDTagger() const
{
  // Captures nothing: the numbering is fixed by the geometry's own construction,
  // so unlike the region tagger it needs no state read back at step time.
  return [](const G4Step * step) -> G4int {
    const G4StepPoint * pre = step->GetPreStepPoint();
    const G4VPhysicalVolume * pv = pre ? pre->GetPhysicalVolume() : nullptr;
    if (!pv) return -1;

    const G4String & name = pv->GetName();
    if (name != "PSMDScintTop" && name != "PSMDScintBottom") return -1;

    const G4TouchableHandle & touch = pre->GetTouchableHandle();
    const G4int slab   = touch->GetCopyNumber(0);
    const G4int module = touch->GetCopyNumber(1);

    // The copy number is the detector's own module number, 1..kModules, with holes.
    if (module < 1 || module > geo::psmd::kModules) return -1;
    if (slab < 0 || slab >= geo::psmd::kSlabsPerModule) return -1;

    return geo::psmd::kSlabsPerModule * module + slab;
  };
}


std::function<G4int(const G4Step *)> DetectorConstruction::MakeRegionTagger() const
{
  // Capture this, NOT the bounds by value: the tagger is normally built while
  // wiring up the run, which happens before /run/initialize calls Construct(),
  // so the bounds are still zero at that point. Reading them through the object
  // defers the lookup to step time, once they are filled in. The run manager owns
  // this detector for the whole run, so the pointer stays valid.
  return [this](const G4Step * step) -> G4int {
    const WaterRegionBounds & bounds = fWaterRegionBounds;

    // This detector only cares where muons went.
    if (std::abs(step->GetTrack()->GetParticleDefinition()->GetPDGEncoding()) != 13)
      return kRegNone;

    const G4String & volName = step->GetPreStepPoint()->GetPhysicalVolume()->GetName();
    if (volName == "DoorWater") return kRegDoor;
    if (volName != "TankWater") return kRegNone;

    const G4TouchableHandle & touch = step->GetPreStepPoint()->GetTouchableHandle();
    const G4ThreeVector localPos = touch->GetHistory()->GetTopTransform().TransformPoint(
        step->GetPreStepPoint()->GetPosition());

    if (localPos.z() > bounds.roomTopZ) return kRegCeiling;

    // Face strips only -- corners (outside in both X and Y) are left untagged, so
    // all four wall regions keep the same area (inner-room face x water thickness).
    const bool outsideX = std::abs(localPos.x()) > bounds.roomHalfX;
    const bool outsideY = std::abs(localPos.y()) > bounds.roomHalfY;
    if (outsideX && !outsideY) return (localPos.x() > 0) ? kRegWallPX : kRegWallMX;
    if (!outsideX && outsideY) return (localPos.y() > 0) ? kRegWallPY : kRegWallMY;
    return kRegNone;
  };
}
