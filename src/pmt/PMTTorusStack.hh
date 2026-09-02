// class PMTTorusStack
//
// A stack of torii sliced normal to the Z axis.  Each torus segment is
// defined by z_edge, rho_edge, and z_o arrays; swept/cross-section radii
// (a/b) are derived.  Fills phi completely.
// Derived from GLG4TorusStack (GenericLAND software library, G.Horton-Smith 1999)

#pragma once

#include <vector>

#include "G4CSGSolid.hh"

class PMTTorusStack : public G4CSGSolid {
public:
  explicit PMTTorusStack(const G4String & pName);
  ~PMTTorusStack() override = default;

  void SetAllParameters(G4int n,                          // number of Z-segments
                        const G4double z_edge[],          // n+1 edges of Z-segments
                        const G4double rho_edge[],        // n+1 dist. from Z-axis
                        const G4double z_o[],             // tor z-origins (n total)
                        PMTTorusStack * inner = nullptr); // inner TorusStack (opt.)

  void ComputeDimensions(G4VPVParameterisation * p, const G4int n,
                         const G4VPhysicalVolume * pRep) override;

  G4bool CalculateExtent(const EAxis pAxis, const G4VoxelLimits & pVoxelLimit,
                         const G4AffineTransform & pTransform, G4double & pmin,
                         G4double & pmax) const override;

  G4double GetN() const { return n; }
  G4double GetZEdge(int i) const { return z_edge[i]; }
  G4double GetZo(int i) const { return z_o[i]; }
  G4double GetA(int i) const { return a[i]; }
  G4double GetB(int i) const { return b[i]; }

  EInside Inside(const G4ThreeVector & p) const override;
  G4ThreeVector SurfaceNormal(const G4ThreeVector & p) const override;

  G4double DistanceToIn(const G4ThreeVector & p, const G4ThreeVector & v) const override;
  G4double DistanceToIn(const G4ThreeVector & p) const override;
  G4double DistanceToOut(const G4ThreeVector & p, const G4ThreeVector & v,
                         const G4bool calcNorm = false, G4bool * validNorm = nullptr,
                         G4ThreeVector * n = nullptr) const override;
  G4double DistanceToOut(const G4ThreeVector & p) const override;

  G4GeometryType GetEntityType() const override { return G4String("PMTTorusStack"); }

  void DescribeYourselfTo(G4VGraphicsScene & scene) const override;
  G4VisExtent GetExtent() const override;
  G4Polyhedron * CreatePolyhedron() const override;

  // find first torus intersection — s == distance from p along v
  static G4int FindFirstTorusRoot(G4double a, G4double b, const G4ThreeVector & p,
                                  const G4ThreeVector & v, G4double smin, G4double smax,
                                  G4bool fEntering, G4double & sout);

  // find first root of arbitrary function in bracketed interval
  class RootFinder {
  public:
    virtual ~RootFinder() = default;
    virtual void f_and_Df(G4double x, G4double & f, G4double & Df) = 0;
    int FindRoot(G4double smin, G4double smax, G4double tol, G4bool fFindFallingRoot,
                 G4double & sout);
  };

  // return integer i such that z[i] <= z_lu < z[i+1], or -1
  static G4int FindInOrderedList(double z_lu, const double * z, int n);

  G4int FindNearestSegment(G4double pr, G4double pz) const;

protected:
  G4ThreeVectorList * CreateRotatedVertices(const G4AffineTransform & pTransform,
                                            G4int & noPolygonVertices) const;
  EInside Inside1(const G4ThreeVector & p) const;
  void CheckABRho();

  int n = 0;
  std::vector<double> z_edge;
  std::vector<double> rho_edge;
  std::vector<double> z_o;
  std::vector<double> a;
  std::vector<double> b;

  double max_rho = 0.0;
  double myRadTolerance = 0.0;

  static double surfaceTolerance;
  static double radTolerance;
  static double angTolerance;

  PMTTorusStack * inner = nullptr;
};
