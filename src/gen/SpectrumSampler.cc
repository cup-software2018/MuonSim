#include <algorithm>
#include <cmath>
#include <utility>

#include "Randomize.hh"
#include "SpectrumSampler.hh"

void SpectrumSampler::Set(const std::vector<G4double> & x, const std::vector<G4double> & density)
{
  const std::size_t n = std::min(x.size(), density.size());

  // Sort by x: a caller-supplied table need not be ascending, and a negative
  // segment width would break the interpolation.
  std::vector<std::pair<G4double, G4double>> points;
  points.reserve(n);
  for (std::size_t i = 0; i < n; i++)
    points.emplace_back(x[i], density[i]);
  std::sort(points.begin(), points.end(),
            [](const auto & a, const auto & b) { return a.first < b.first; });

  fX.clear();
  fDensity.clear();
  fX.reserve(n);
  fDensity.reserve(n);
  for (const auto & p : points) {
    fX.push_back(p.first);
    fDensity.push_back(p.second);
  }

  BuildCDF();
}

void SpectrumSampler::SetFromFunction(const std::function<G4double(G4double)> & f, G4double lo,
                                      G4double hi, std::size_t nBins)
{
  if (nBins == 0 || hi <= lo) return;

  std::vector<G4double> x(nBins + 1), density(nBins + 1);
  const G4double step = (hi - lo) / (G4double)nBins;
  for (std::size_t i = 0; i <= nBins; i++) {
    x[i] = lo + (G4double)i * step;
    density[i] = std::max(0., f(x[i])); // a fit can dip slightly negative
  }
  Set(x, density);
}

void SpectrumSampler::BuildCDF()
{
  fCDF.assign(fX.size(), 0.);
  for (std::size_t i = 0; i + 1 < fX.size(); i++) {
    const G4double area = 0.5 * (fDensity[i] + fDensity[i + 1]) * (fX[i + 1] - fX[i]);
    fCDF[i + 1] = fCDF[i] + area;
  }

  const G4double total = fCDF.empty() ? 0. : fCDF.back();
  if (total > 0.) {
    for (auto & v : fCDF)
      v /= total;
  }
}

G4double SpectrumSampler::Sample() const
{
  if (fX.empty()) return 0.;
  if (fX.size() == 1 || fCDF.back() <= 0.) return fX.front();

  const G4double r = G4UniformRand();

  // Segment k spans [fX[k], fX[k+1]].
  std::size_t k = (std::size_t)(std::upper_bound(fCDF.begin(), fCDF.end(), r) - fCDF.begin());
  if (k == 0) k = 1;
  if (k >= fX.size()) k = fX.size() - 1;
  k -= 1;

  const G4double x1 = fX[k];
  const G4double h = fX[k + 1] - x1;
  if (h <= 0.) return x1;

  const G4double segment = fCDF[k + 1] - fCDF[k];
  if (segment <= 0.) return x1;

  const G4double a = fDensity[k];
  const G4double b = fDensity[k + 1];

  // Within the segment the density rises linearly a -> b, so the mass covered
  // up to t is  a*t + (b-a)*t^2/2  (per unit length). Invert that for t.
  const G4double frac = (r - fCDF[k]) / segment; // 0..1 of this segment's mass
  const G4double mass = frac * 0.5 * (a + b);    // per unit length

  G4double t = 0.;
  if (std::abs(b - a) < 1.e-12 * std::max(1., std::abs(a))) {
    t = (a > 0.) ? mass / a : frac; // flat segment -> uniform
  }
  else {
    const G4double disc = a * a + 2. * (b - a) * mass;
    t = (disc > 0.) ? (-a + std::sqrt(disc)) / (b - a) : frac;
  }

  if (t < 0.) t = 0.;
  if (t > 1.) t = 1.;

  return x1 + t * h;
}

G4double SpectrumSampler::Density(G4double x) const
{
  if (fX.empty()) return 0.;
  if (x < fX.front() || x > fX.back()) return 0.;

  const auto it = std::upper_bound(fX.begin(), fX.end(), x);
  if (it == fX.begin()) return fDensity.front();
  if (it == fX.end()) return fDensity.back();

  const std::size_t k = (std::size_t)(it - fX.begin()) - 1;
  const G4double h = fX[k + 1] - fX[k];
  if (h <= 0.) return fDensity[k];
  const G4double t = (x - fX[k]) / h;
  return fDensity[k] + t * (fDensity[k + 1] - fDensity[k]);
}

G4double SpectrumSampler::MaxDensity() const
{
  if (fDensity.empty()) return 0.;
  return *std::max_element(fDensity.begin(), fDensity.end());
}

G4double SpectrumSampler::Mean() const
{
  G4double numerator = 0., denominator = 0.;
  for (std::size_t i = 0; i + 1 < fX.size(); i++) {
    const G4double x1 = fX[i];
    const G4double h = fX[i + 1] - x1;
    const G4double a = fDensity[i];
    const G4double b = fDensity[i + 1];
    denominator += 0.5 * (a + b) * h;
    numerator += h * (x1 * a + x1 * (b - a) / 2. + h * a / 2. + h * (b - a) / 3.);
  }
  return (denominator > 0.) ? numerator / denominator : 0.;
}
