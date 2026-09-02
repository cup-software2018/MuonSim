#pragma once

#include <string>
#include <vector>

#include "G4Types.hh"

// Optical properties of materials and surfaces, read from YAML.
//
// Why a file rather than a header: these tables ARE the systematics of an optical
// detector -- water attenuation, reflector reflectivity, photocathode efficiency.
// "What if the attenuation length is 20% worse" has to be runnable as a scan from
// a macro, which a compiled-in table cannot be. Dimensions stay in code, because
// they interlock and are not something anyone scans.
//
// Schema -- a top-level list, each entry naming one material or one surface:
//
//   - material: G4_WATER                 # or  surface: TyvekSurface
//     provenance:                        # optional, but say where numbers came from
//       source: "..."
//       note: "..."
//     properties:
//       RINDEX:
//         energy_unit: eV                # or wavelength_unit: nm
//         value_unit: mm                 # omit when the property is dimensionless
//         energy: [ ... ]                # or wavelength: [ ... ]
//         value:  [ ... ]                # same length
//       SCINTILLATIONCOMPONENT1:
//         wavelength_unit: nm
//         spectrum: true                 # see below
//         wavelength: [ ... ]
//         value:      [ ... ]
//       THICKNESS:
//         new_key: true                  # required for non-standard G4 keys
//         ...
//     constants:                         # AddConstProperty: not functions of energy
//       SCINTILLATIONYIELD: 10000
//       SCINTILLATIONTIMECONSTANT1: 2.1 ns
//     birks_constant: 0.126 mm/MeV       # NOT a properties-table entry: it lives on
//                                        # the material's ionisation parameters
//
// Wavelength input exists because datasheets are quoted in nm. Geant4 wants photon
// ENERGY, ascending; converting from ascending wavelength reverses the order, which
// is a standing source of silently wrong tables. The loader converts and sorts.
//
// spectrum: true marks a density -- counts per unit interval, i.e. an emission
// spectrum. Changing variable then needs the Jacobian, dN/dE = dN/dlambda *
// lambda^2, or the shape comes out skewed to the blue; between 400 and 500 nm that
// factor is 1.56. It must NOT be applied to RINDEX, ABSLENGTH or REFLECTIVITY,
// which are plain values rather than densities.
//
// Unknown property keys are rejected unless new_key is set. Geant4's AddProperty
// would happily accept a typo as a brand-new property that then does nothing.

// Remembers a file to be applied later. Materials and surfaces do not exist until
// the geometry is built, so the file cannot be applied when it is named -- only
// afterwards, from the detector construction. Naming one after that point is a
// fatal error rather than a no-op; see the note on the definition.
void RegisterMaterialPropertyFile(const std::string & path);

const std::vector<std::string> & RegisteredMaterialPropertyFiles();

// Applies every registered file. Call once, after the materials and optical
// surfaces have been created. Returns false with a filled-in error on the first
// problem, leaving whatever was already applied in place.
bool ApplyMaterialPropertyFiles(std::string & error);

// Applies one file. Exposed for a detector that would rather name its own.
bool LoadMaterialPropertiesYaml(const std::string & path, std::string & error);

// A short content digest of the last file applied, for the record: results depend
// on these numbers, so a run should be able to say which ones it used.
const std::string & MaterialPropertyFileDigest();
