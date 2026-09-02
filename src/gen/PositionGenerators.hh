#pragma once

#include <memory>
#include <string>
#include <vector>

#include "AbsPosGen.hh"
#include "G4AffineTransform.hh"
#include "G4ThreeVector.hh"
#include "G4Types.hh"

class G4VSolid;
class G4VPhysicalVolume;
class G4Navigator;

// Local->global transform and solid of a named physical volume, resolved once
// on first use (the geometry is static after initialization).
struct ResolvedVolume {
  G4AffineTransform transform;
  G4VSolid * solid = nullptr;
  // The placement itself, and the world to navigate from. Needed because a
  // volume's solid is NOT the space the volume occupies: daughters are carved
  // out of it, and only the navigator knows about them.
  G4VPhysicalVolume * physical = nullptr;
  G4VPhysicalVolume * world = nullptr;
  bool resolved = false;
  bool valid = false;

  // who: caller name, used in the "volume not found" warning.
  void EnsureResolved(const std::string & volumeName, const char * who);
};

// Fixed position, or a random pick among several fixed positions (uniform)
// when constructed with more than one.
class PointPosGen : public AbsPosGen {
public:
  PointPosGen() = default;
  PointPosGen(const G4ThreeVector & position);
  explicit PointPosGen(const std::vector<G4ThreeVector> & positions);

  ~PointPosGen() override = default;

  const std::string & GetKey() const override;
  PosDir Generate() override;

private:
  std::vector<G4ThreeVector> fPositions;
};

class InVolumePosGen : public AbsPosGen {
public:
  InVolumePosGen() = default;
  InVolumePosGen(const std::string & volumeName);

  ~InVolumePosGen() override = default;

  const std::string & GetKey() const override;
  PosDir Generate() override;

private:
  std::string fVolumeName;
  ResolvedVolume fTarget;

  // Ours, not G4TransportationManager's tracking navigator: relocating that one
  // outside of tracking is asking for trouble. Built on first use.
  // shared, not unique: MultiVolumePosGen keeps these in a vector, and a unique_ptr
  // would make the class move-only. Every lookup is a full search, so sharing one
  // navigator between generators cannot change an answer.
  std::shared_ptr<G4Navigator> fProbe;
};

class OnVolumePosGen : public AbsPosGen {
public:
  OnVolumePosGen() = default;
  OnVolumePosGen(const std::string & volumeName);

  ~OnVolumePosGen() override = default;

  const std::string & GetKey() const override;
  PosDir Generate() override;

  // Restrict the emission to one side of the surface, and choose how it spreads
  // over that hemisphere. With Side::Both no direction is dictated at all.
  enum class Side { Both, In, Out };
  void SetSide(Side side) { fSide = side; }
  void SetAngularLaw(AngularLaw law) { fLaw = law; }

  static Side ParseSide(const std::string & token, bool & ok);
  static const char * SideName(Side side);

private:
  std::string fVolumeName;
  ResolvedVolume fTarget;
  Side fSide = Side::Both;
  AngularLaw fLaw = AngularLaw::Isotropic;
};

// Picks one of several volumes at random, weighted by each volume's cubic
// volume unless explicit weights are given, then samples inside it
// (InVolumePosGen). Useful for bulk radioactive background spread across
// multiple detector components (e.g. steel + PMT glass) as a single source.
class MultiVolumePosGen : public AbsPosGen {
public:
  MultiVolumePosGen() = default;
  explicit MultiVolumePosGen(const std::vector<std::string> & volumeNames);
  MultiVolumePosGen(const std::vector<std::string> & volumeNames,
                    const std::vector<G4double> & weights);
  // Matches every physical volume whose name contains namePattern as a
  // substring (e.g. "PMTPhys" -> PMTPhys0..PMTPhys47), each auto-weighted.
  explicit MultiVolumePosGen(const std::string & namePattern);

  ~MultiVolumePosGen() override = default;

  const std::string & GetKey() const override;
  PosDir Generate() override;

private:
  void EnsureBuilt();

  std::vector<std::string> fVolumeNames;
  std::string fNamePattern;       // if non-empty, resolves fVolumeNames by substring match
  std::vector<G4double> fWeights; // explicit weights; empty => auto (cubic volume)

  bool fBuilt = false;
  std::vector<InVolumePosGen> fGenerators; // one per volume, built lazily
  std::vector<G4double> fCDF;              // normalised cumulative weights, size = n+1
};

