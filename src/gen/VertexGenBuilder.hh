#pragma once

#include <memory>
#include <string>

class AbsVertexGen;

// Builds a vertex generator from one /gen/vertex command line:
//
//   <type> [<energy>] [clause]...
//
// The type comes first, an optional kinetic-energy spec may follow it directly,
// and the rest are keyword-led clauses in any order.
//
// This is plain text parsing -- it knows nothing about G4UImessenger or
// G4UIcommand, so the generator model stays usable without the Geant4 UI.
// Malformed input raises a FatalException naming the offending token.
std::unique_ptr<AbsVertexGen> BuildVertexGen(const std::string & line);

// Names of the sources that bring their own particles, for guidance and errors.
std::string SourceTypeNames();
