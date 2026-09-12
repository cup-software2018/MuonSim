#include <algorithm>
#include <memory>
#include <sstream>

#include "G4AffineTransform.hh"
#include "G4Exception.hh"
#include "G4LogicalVolume.hh"
#include "G4Navigator.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4TransportationManager.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VSolid.hh"
#include "PositionGenerators.hh"
#include "G4PhysicalConstants.hh"
#include "Randomize.hh"

// ============================================================================
// PointPosGen
// ============================================================================

PointPosGen::PointPosGen(const G4ThreeVector & position)
  : fPositions(1, position)
{
}

PointPosGen::PointPosGen(const std::vector<G4ThreeVector> & positions)
  : fPositions(positions)
{
}

const std::string & PointPosGen::GetKey() const
{
  static const std::string key = "Point";
  return key;
}

PosDir PointPosGen::Generate()
{
  PosDir out;
  if (fPositions.empty()) return out;
  std::size_t idx = (std::size_t)(G4UniformRand() * fPositions.size());
  if (idx >= fPositions.size()) idx = fPositions.size() - 1;
  out.position = fPositions[idx];
  return out;
}

// ============================================================================
// InVolumePosGen
// ============================================================================

InVolumePosGen::InVolumePosGen(const std::string & volumeName)
  : fVolumeName(volumeName)
{
}

const std::string & InVolumePosGen::GetKey() const
{
  static const std::string key = "InVolume";
  return key;
}

namespace
{
// Accumulates the local->global transform of targetName.
//
// Order matters: G4AffineTransform's TransformPoint(v) is v*rot + tlate, so
// A * B means "apply A, then B". Going from a daughter's local frame out to
// the world therefore composes daughter-first: daughterTransform * motherLocal.
// (Getting this backwards is invisible for pure-translation chains and only
// shows up once a rotation is involved -- e.g. the 180-degree-rotated PMTs.)
bool FindTransform(G4VPhysicalVolume * currentPV, const G4String & targetName,
                   G4AffineTransform & transform)
{
  if (!currentPV) return false;

  const G4AffineTransform localTransform(currentPV->GetObjectRotation(),
                                         currentPV->GetObjectTranslation());

  if (currentPV->GetName() == targetName) {
    transform = localTransform;
    return true;
  }

  G4LogicalVolume * lv = currentPV->GetLogicalVolume();
  if (lv) {
    const auto nDaughters = lv->GetNoDaughters();
    for (decltype(lv->GetNoDaughters()) i = 0; i < nDaughters; i++) {
      G4AffineTransform daughterTransform;
      if (FindTransform(lv->GetDaughter(i), targetName, daughterTransform)) {
        transform = daughterTransform * localTransform;
        return true;
      }
    }
  }
  return false;
}
} // namespace

void ResolvedVolume::EnsureResolved(const std::string & volumeName, const char * who)
{
  if (resolved) return;
  resolved = true;

  auto * transportMgr = G4TransportationManager::GetTransportationManager();
  auto * navigator = transportMgr ? transportMgr->GetNavigatorForTracking() : nullptr;
  auto * world_ = navigator ? navigator->GetWorldVolume() : nullptr;
  if (!world_) {
    G4Exception(who, "GEN001", JustWarning,
                "No world volume yet -- has /run/initialize been called? Returning origin.");
    return;
  }

  if (!FindTransform(world_, volumeName, transform)) {
    G4Exception(who, "GEN002", JustWarning,
                ("Physical volume not found: " + volumeName + " -- returning origin").c_str());
    return;
  }

  auto * targetPV = G4PhysicalVolumeStore::GetInstance()->GetVolume(volumeName);
  physical = targetPV;
  world = world_;
  G4LogicalVolume * lv = targetPV ? targetPV->GetLogicalVolume() : nullptr;
  solid = lv ? lv->GetSolid() : nullptr;
  if (!solid) {
    G4Exception(who, "GEN003", JustWarning,
                ("No solid for physical volume: " + volumeName + " -- returning origin").c_str());
    return;
  }

  valid = true;
}

// Uniform in the space the volume actually OCCUPIES, which is its solid minus
// whatever is placed inside it.
//
// The solid alone is not enough, and the difference is not a detail. A volume's
// solid is its outer shape; every daughter placed in it carves material out, and
// G4VSolid::Inside() knows nothing about daughters. Rock is a shell wrapping the
// hall, so its solid is the whole ball: sampling that put 74% of the decays in
// hall AIR -- exactly the hall's share of the Rock solid's volume, 74.8% -- and
// 96% of the "escaping" gammas that came out of that run were air decays
// backscattering off the rock. The volume looked right, the numbers did not.
//
// So there are two stages: reject against the solid (cheap, local), then ask the
// navigator what is really at that point and reject unless it is the target
// (authoritative, and the only thing that sees daughters). Same two stages as
// RAT-PAC's GLG4PosGen_Fill.
PosDir InVolumePosGen::Generate()
{
  PosDir out;
  fTarget.EnsureResolved(fVolumeName, "InVolumePosGen::Generate");
  if (!fTarget.valid) return out;

  G4VSolid * solid = fTarget.solid;

  // Ours rather than the tracking navigator: relocating that one outside of
  // tracking disturbs state that Geant4 expects to own.
  if (!fProbe) {
    fProbe = std::make_shared<G4Navigator>();
    fProbe->SetWorldVolume(fTarget.world);
  }

  G4ThreeVector pMin, pMax;
  solid->BoundingLimits(pMin, pMax);

  // Generous, because the daughter rejection lowers the acceptance -- for Rock
  // from 42% of its bounding box to 11%. Running out is a geometry problem, not
  // a piece of bad luck, so it ends the run instead of returning a point that is
  // somewhere else entirely. The old code quietly used the local origin, which
  // for a shell like Rock is the middle of the hall it wraps.
  const int kMaxAttempts = 100000;

  G4ThreeVector localPos, globalPos;
  int attempts = 0;
  bool found = false;
  while (attempts < kMaxAttempts) {
    attempts++;
    localPos = G4ThreeVector(pMin.x() + G4UniformRand() * (pMax.x() - pMin.x()),
                             pMin.y() + G4UniformRand() * (pMax.y() - pMin.y()),
                             pMin.z() + G4UniformRand() * (pMax.z() - pMin.z()));
    if (solid->Inside(localPos) != kInside) continue;

    globalPos = fTarget.transform.TransformPoint(localPos);
    // Full search, not relative: consecutive samples are uncorrelated, so there
    // is no previous location worth starting from.
    const G4VPhysicalVolume * here = fProbe->LocateGlobalPointAndSetup(globalPos, nullptr, false, true);
    if (here == fTarget.physical) { found = true; break; }
  }

  if (!found) {
    std::ostringstream msg;
    msg << "gave up after " << kMaxAttempts << " attempts looking for a point inside '"
        << fVolumeName << "'. Its daughters may fill it completely, or it may be too "
        << "thin a shell to hit by sampling its bounding box.";
    G4Exception("InVolumePosGen::Generate", "GEN004", FatalException, msg.str().c_str());
  }

  out.position = globalPos;
  return out;
}

// ============================================================================
// OnVolumePosGen
// ============================================================================

OnVolumePosGen::OnVolumePosGen(const std::string & volumeName)
  : fVolumeName(volumeName)
{
}

const std::string & OnVolumePosGen::GetKey() const
{
  static const std::string key = "OnVolume";
  return key;
}

OnVolumePosGen::Side OnVolumePosGen::ParseSide(const std::string & token, bool & ok)
{
  std::string lower = token;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  ok = true;
  if (lower == "both") return Side::Both;
  if (lower == "in" || lower == "inward" || lower == "inside") return Side::In;
  if (lower == "out" || lower == "outward" || lower == "outside") return Side::Out;
  ok = false;
  return Side::Both;
}

const char * OnVolumePosGen::SideName(Side side)
{
  switch (side) {
    case Side::In: return "in";
    case Side::Out: return "out";
    default: return "both";
  }
}

PosDir OnVolumePosGen::Generate()
{
  PosDir out;
  fTarget.EnsureResolved(fVolumeName, "OnVolumePosGen::Generate");
  if (!fTarget.valid) return out;

  const G4ThreeVector localPos = fTarget.solid->GetPointOnSurface();
  out.position = fTarget.transform.TransformPoint(localPos);

  // SurfaceNormal() is in the solid's own frame, so rotate it -- TransformAxis
  // applies the rotation without the translation.
  out.normal = fTarget.transform.TransformAxis(fTarget.solid->SurfaceNormal(localPos)).unit();
  out.hasNormal = true;

  if (fSide != Side::Both) {
    const G4ThreeVector emissionNormal = (fSide == Side::In) ? -out.normal : out.normal;
    out.direction = HemisphereDirection(emissionNormal, fLaw);
    out.hasDirection = true;
  }
  return out;
}

// ============================================================================
// MultiVolumePosGen
// ============================================================================

MultiVolumePosGen::MultiVolumePosGen(const std::vector<std::string> & volumeNames)
  : fVolumeNames(volumeNames)
{
}

MultiVolumePosGen::MultiVolumePosGen(const std::vector<std::string> & volumeNames,
                                     const std::vector<G4double> & weights)
  : fVolumeNames(volumeNames),
    fWeights(weights)
{
  if (fWeights.size() != fVolumeNames.size()) {
    G4Exception("MultiVolumePosGen::MultiVolumePosGen", "GEN201", JustWarning,
                "weights.size() != volumeNames.size() -- falling back to cubic-volume weights");
    fWeights.clear();
  }
}

MultiVolumePosGen::MultiVolumePosGen(const std::string & namePattern)
  : fNamePattern(namePattern)
{
}

const std::string & MultiVolumePosGen::GetKey() const
{
  static const std::string key = "MultiVolume";
  return key;
}

void MultiVolumePosGen::EnsureBuilt()
{
  if (fBuilt) return;
  fBuilt = true;

  if (!fNamePattern.empty()) {
    for (auto * pv : *G4PhysicalVolumeStore::GetInstance()) {
      if (pv && pv->GetName().find(fNamePattern) != std::string::npos)
        fVolumeNames.push_back(pv->GetName());
    }
    if (fVolumeNames.empty()) {
      // Fatal, where this used to warn. A pattern matching nothing left Generate()
      // with no generators, and it returned a default PosDir -- every vertex at the
      // world origin, which here is inside the rock. The run then completes, writes
      // a file and reports no light, and the only trace is one JustWarning line
      // thousands of lines up the log. The matching is a plain substring and it is
      // CASE SENSITIVE, so 'water' instead of 'Water' is all it takes.
      std::ostringstream msg;
      msg << "no physical volume name contains '" << fNamePattern
          << "'. The match is a case-sensitive substring of the PHYSICAL volume name"
          << " -- 'Water' finds TankWater and DoorWater, 'water' finds nothing."
          << " Run with /control/listAlias and check the name against the geometry.";
      G4Exception("MultiVolumePosGen::EnsureBuilt", "GEN203", FatalException,
                  msg.str().c_str());
    }
  }

  fGenerators.reserve(fVolumeNames.size());
  for (const auto & name : fVolumeNames)
    fGenerators.emplace_back(name);

  std::vector<G4double> weights = fWeights;
  if (weights.empty()) {
    weights.reserve(fVolumeNames.size());
    for (const auto & name : fVolumeNames) {
      G4double cubicVolume = 0.;
      auto * pv = G4PhysicalVolumeStore::GetInstance()->GetVolume(name);
      G4LogicalVolume * lv = pv ? pv->GetLogicalVolume() : nullptr;
      G4VSolid * solid = lv ? lv->GetSolid() : nullptr;
      if (solid) { cubicVolume = solid->GetCubicVolume(); }
      else {
        G4Exception("MultiVolumePosGen::EnsureBuilt", "GEN202", JustWarning,
                    ("Physical volume not found: " + name + " -- weight 0").c_str());
      }
      weights.push_back(cubicVolume);
    }
  }

  fCDF.assign(weights.size() + 1, 0.);
  for (std::size_t i = 0; i < weights.size(); i++)
    fCDF[i + 1] = fCDF[i] + weights[i];

  const G4double total = fCDF.back();
  if (total > 0.) {
    for (auto & v : fCDF)
      v /= total;
  }
  else {
    // No usable weights (e.g. no volumes found) -- fall back to a uniform split.
    for (std::size_t i = 0; i <= weights.size(); i++)
      fCDF[i] = weights.empty() ? 0. : (G4double)i / (G4double)weights.size();
  }
}

PosDir MultiVolumePosGen::Generate()
{
  EnsureBuilt();
  // Only reachable through the explicit-name-list constructors now: a pattern that
  // matches nothing is fatal in EnsureBuilt, so it can no longer arrive here and
  // put every vertex at the origin.
  if (fGenerators.empty()) return PosDir();

  const G4double r = G4UniformRand();
  std::size_t idx = (std::size_t)(std::upper_bound(fCDF.begin(), fCDF.end(), r) - fCDF.begin()) - 1;
  if (idx >= fGenerators.size()) idx = fGenerators.size() - 1;

  return fGenerators[idx].Generate();
}
