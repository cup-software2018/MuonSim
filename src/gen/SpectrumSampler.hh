#pragma once

#include <functional>
#include <vector>

#include "G4Types.hh"

// Samples a continuous distribution tabulated as (x, density) points, treating
// the density as piecewise linear between them and inverting the exact
// trapezoidal CDF. Correct for unevenly spaced points, unlike a discrete
// per-point weighting, which would over-represent densely sampled regions.
//
// Shared by the source generators in this directory (AmBeGen, CfGen, ...) so the
// inverse-CDF algebra lives in one place.
class SpectrumSampler {
public:
  SpectrumSampler() = default;

  // Points need not be sorted, and the density need not be normalised.
  void Set(const std::vector<G4double> & x, const std::vector<G4double> & density);

  // Tabulates f over [lo, hi] on nBins + 1 uniformly spaced points. Use for a
  // spectrum given as a formula rather than as measured points.
  void SetFromFunction(const std::function<G4double(G4double)> & f, G4double lo, G4double hi,
                       std::size_t nBins);

  bool Empty() const { return fX.empty(); }

  G4double Sample() const;

  // Piecewise-linear value of the (unnormalised) density at x; 0 outside the
  // tabulated range. For weighting rather than sampling -- e.g. a flux factor
  // inside a rejection loop.
  G4double Density(G4double x) const;

  G4double MinX() const { return fX.empty() ? 0. : fX.front(); }
  G4double MaxX() const { return fX.empty() ? 0. : fX.back(); }
  G4double MaxDensity() const;

  // Mean of the tabulated distribution -- a cheap cross-check on the table.
  G4double Mean() const;

private:
  void BuildCDF();

  std::vector<G4double> fX;       // ascending
  std::vector<G4double> fDensity; // same order as fX
  std::vector<G4double> fCDF;     // normalised cumulative trapezoid integrals
};
