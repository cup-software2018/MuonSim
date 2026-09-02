#pragma once

#include "G4LogicalVolume.hh"

class G4Material;
class G4OpticalSurface;

// PMTLogicalVolume is parent class for specific tube classes below
class PMTLogicalVolume : public G4LogicalVolume {
public:
  PMTLogicalVolume(const G4String & plabel,   // label -- subvolume names are derived from this
                   G4double r_bound,          // radius of bounding cylinder
                   G4double hh_bound,         // half height of bounding cylinder
                   G4Material * ExteriorMat); // material which fills the bounding cylinder

  G4double GetZEquator() const { return z_equator; }

protected:
  G4bool isvisible;
  G4double z_equator; // Z location of equator of tube

  static G4OpticalSurface * our_Mirror_opsurf;

  void
  ConstructPMT_UsingTorusStack(const G4int n_edge, const G4double outer_z_edge[],
                               const G4double outer_rho_edge[], const G4double outer_z_o[],
                               G4double r_dynode, // radius of dynode stack
                               G4double z_dynode, // z coordinate of top of dynode stack, equator=0.
                               G4double d_wall,   // thickness of the walls
                               G4Material * Exterior,                  // material outside tube
                               G4Material * Glass,                     // glass material
                               G4OpticalSurface * Photocathode_opsurf, // photocathode surface
                               G4Material * PMT_Vac,                   // tube interior
                               G4Material * Dynode_mat,                // dynode stack metal
                               G4VSensitiveDetector * detector         // sensitive detector hook
  );
};

// Hamamatsu R7250 PMT (so-called "17-inch") with optional face mask:
// note that the R7250 has the same outside shape and size as the
// R3600 "20-inch" PMT, but has less usable photocathode area.
class PMT17inchLogicalVolume : public PMTLogicalVolume {
public:
  PMT17inchLogicalVolume(
      const G4String & plabel,                // label -- subvolume names are derived from this
      G4Material * ExteriorMat,               // material which fills the bounding cylinder
      G4Material * GlassMat,                  // glass material
      G4OpticalSurface * Photocathode_opsurf, // photocathode surface
      G4Material * PMT_Vacuum,                // vacuum inside tube
      G4Material * DynodeMat,                 // dynode material
      G4Material * MaskMat,                   // material for photocathode mask (e.g, blk acryl)
                                              // OK to set MaskMat == nullptr for no mask
      G4VSensitiveDetector * detector         // sensitive detector hook
  );
};

// Hamamatsu R3600 ("20-inch") PMT with optional face mask
class PMT20inchLogicalVolume : public PMTLogicalVolume {
public:
  PMT20inchLogicalVolume(
      const G4String & plabel,                // label -- subvolume names are derived from this
      G4Material * ExteriorMat,               // material which fills the bounding cylinder
      G4Material * GlassMat,                  // glass material
      G4OpticalSurface * Photocathode_opsurf, // photocathode surface
      G4Material * PMT_Vacuum,                // vacuum inside tube
      G4Material * DynodeMat,                 // dynode material
      G4Material * MaskMat,                   // material for photocathode mask (e.g, blk acryl)
                                              // OK to set MaskMat == nullptr for no mask
      G4VSensitiveDetector * detector         // sensitive detector hook
  );
};

// Hamamatsu R7081 ("10-inch") PMT with optional face mask
class PMT10inchLogicalVolume : public PMTLogicalVolume {
public:
  PMT10inchLogicalVolume(
      const G4String & plabel,                // label -- subvolume names are derived from this
      G4Material * ExteriorMat,               // material which fills the bounding cylinder
      G4Material * GlassMat,                  // glass material
      G4OpticalSurface * Photocathode_opsurf, // photocathode surface
      G4Material * PMT_Vacuum,                // vacuum inside tube
      G4Material * DynodeMat,                 // dynode material
      G4Material * MaskMat,                   // material for photocathode mask (e.g, blk acryl)
                                              // OK to set MaskMat == nullptr for no mask
      G4VSensitiveDetector * detector         // sensitive detector hook
  );
};

// Hamamatsu R1408 ("8-inch") PMT with optional face mask
//  (note R5912 and R4558 use same glass envelope, but dynode may be different)
class PMT8inchLogicalVolume : public PMTLogicalVolume {
public:
  PMT8inchLogicalVolume(const G4String & plabel, // label -- subvolume names are derived from this
                          G4Material * ExteriorMat, // material which fills the bounding cylinder
                          G4Material * GlassMat,    // glass material
                          G4OpticalSurface * Photocathode_opsurf, // photocathode surface
                          G4Material * PMT_Vacuum,                // vacuum inside tube
                          G4Material * DynodeMat,                 // dynode material
                          G4Material * MaskMat, // material for photocathode mask (e.g, blk acryl)
                                                // OK to set MaskMat == nullptr for no mask
                          G4VSensitiveDetector * detector // sensitive detector hook
  );
};

class PMT5inchLogicalVolume : public PMTLogicalVolume {
public:
  PMT5inchLogicalVolume(const G4String & plabel, // label -- subvolume names are derived from this
                          G4Material * ExteriorMat, // material which fills the bounding cylinder
                          G4Material * GlassMat,    // glass material
                          G4OpticalSurface * Photocathode_opsurf, // photocathode surface
                          G4Material * PMT_Vacuum,                // vacuum inside tube
                          G4Material * DynodeMat,                 // dynode material
                          G4Material * MaskMat, // material for photocathode mask (e.g, blk acryl)
                                                // OK to set MaskMat == nullptr for no mask
                          G4VSensitiveDetector * detector // sensitive detector hook
  );
};
