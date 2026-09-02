#pragma once

#include <map>
#include <utility>
#include <vector>

#include "G4LogicalBorderSurface.hh"
#include "G4LogicalVolume.hh"
#include "G4MaterialPropertyVector.hh"
#include "G4OpticalSurface.hh"
#include "G4UImessenger.hh"
#include "G4VFastSimulationModel.hh"
#include "G4ios.hh"

class G4UIcommand;
class G4UIdirectory;
class G4Region;

class PMTOpticalModel : public G4VFastSimulationModel, public G4UImessenger {
public:
  PMTOpticalModel(G4String, G4Region *, G4LogicalVolume * body, G4OpticalSurface * op_surface,
                  double efficiency_correction = 1.0, double dynodeTop = 0.0,
                  double dynodeRadius = 0.0, double prepulseProb = 0.0,
                  double photocathode_MINrho = 0.0, double photocathode_MAXrho = 0.0);
  ~PMTOpticalModel() override = default;

  G4bool IsApplicable(const G4ParticleDefinition &) override;
  G4bool ModelTrigger(const G4FastTrack &) override;
  void DoIt(const G4FastTrack &, G4FastStep &) override;

  void SetNewValue(G4UIcommand * command, G4String newValues) override;
  G4String GetCurrentValue(G4UIcommand * command) override;

  void SetEfficiencyCorrection(std::map<int, double> effCorr)
  {
    EfficiencyCorrection = std::move(effCorr);
    G4cout << GetName() << ": Individual efficiency correction table set" << G4endl;
  }
  void DumpEfficiencyCorrectionTable()
  {
    G4cout << "Individual correction table for the PMT efficiencies of " << GetName()
           << ":\nPMT ID  corr. factor" << G4endl;
    for (const auto & [id, corr] : EfficiencyCorrection)
      G4cout << id << "," << corr << G4endl;
  }
  void fillPMTVector(double code, double A, double An, double T, double R, double collection_eff,
                     double N_pe, double x, double y, double z, double gx, double gy, double gz,
                     double wavelength, double time)
  {
    pmtHitVectorIndex = {code, A, An, T,  R,  collection_eff, N_pe, x,
                         y,    z, gx, gy, gz, wavelength,     time};
    pmtHitVector.push_back(pmtHitVectorIndex);
    pmtHitVectorIndex.clear();
  }
  [[nodiscard]] std::vector<std::vector<double>> GetPMTVector() const { return pmtHitVector; }
  static std::vector<std::vector<double>> pmtHitVector;

private:
  // material property vectors — owned by the material, not us
  G4MaterialPropertyVector * _rindex_glass = nullptr;
  G4MaterialPropertyVector * _rindex_photocathode = nullptr;
  G4MaterialPropertyVector * _kindex_photocathode = nullptr; // n~ = n+ik, as in M.D.Lay
  G4MaterialPropertyVector * _efficiency_photocathode = nullptr;
  G4MaterialPropertyVector * _thickness_photocathode = nullptr;

  // inner vacuum solid and physical volumes, initialized in constructor
  G4VSolid * _inner1_solid = nullptr;
  G4VPhysicalVolume * _inner1_phys = nullptr;
  G4VPhysicalVolume * _inner2_phys = nullptr;
  G4VPhysicalVolume * _central_gap_phys = nullptr;

  G4int _luxlevel = 3;
  G4double _rho = 0.0;
  G4double _rhoAvg = 0.0;
  G4double _rhoDif = 0.0;
  G4double _erfProb = 0.0;
  G4double _photocathode_MINrho = 0.0;
  G4double _photocathode_MAXrho = 0.0;
  G4double _efficiency_correction = 1.0;
  G4double _dynodeTop = 0.0;
  G4double _dynodeRadius = 0.0;
  G4double _prepulseProb = 0.0;
  G4bool _applyCorrection = true;
  G4int _verbosity = 0;

  static G4UIdirectory * fgCmdDir;

  // per-photon working variables, cached to avoid repeated lookup in DoIt
  G4double _photon_energy = -1.0;
  G4double _wavelength = 0.0;
  G4double _n1 = 0.0, _n2 = 0.0, _k2 = 0.0, _n3 = 0.0;
  G4double _efficiency = 0.0;
  G4double _thickness = 0.0;
  G4double _cos_theta1 = 0.0;
  // set by CalculateCoefficients(), used by DoIt/Reflect/Refract
  G4double _sin_theta1 = 0.0;
  G4double _sin_theta3 = 0.0;
  G4double _cos_theta3 = 0.0;
  G4double fR_s = 0.0, fT_s = 0.0;
  G4double fR_p = 0.0, fT_p = 0.0;
  G4double fR_n = 0.0, fT_n = 0.0;

  void CalculateCoefficients();
  void Reflect(G4ThreeVector & dir, G4ThreeVector & pol, G4ThreeVector & norm);
  void Refract(G4ThreeVector & dir, G4ThreeVector & pol, G4ThreeVector & norm);
  int GetPMTID(const G4FastTrack & fastTrack);

  static double surfaceTolerance;

  std::map<int, double> EfficiencyCorrection;
  std::vector<double> pmtHitVectorIndex;
};
