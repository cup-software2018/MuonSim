#pragma once

#include <string>

// Finds a file that ships with the installation.
//
// Data files have to travel with the binaries, and be found without anyone having
// to source a script first. Resolution order:
//
//   1. the path as given, if it exists          -- an explicit path always wins
//   2. $MUONSIM_DATA/<relative>                 -- set by setup_muonsim.sh
//   3. <dir of the running executable>/../data/ -- makes the install relocatable
//   4. the data directory compiled in at build time
//
// Step 3 is what lets a copied install tree work with no configuration at all. It
// does not help a downstream project that links this library into its own binary --
// there the executable is somewhere else entirely -- which is what steps 2 and 4
// are for.
std::string ResolveDataFile(const std::string & relative);
