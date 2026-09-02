cmake_minimum_required ( VERSION 3.16...3.21 )
project ( MuonSim )

set ( CMAKE_EXPORT_COMPILE_COMMANDS ON )
set ( CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE )

# ----------------------------------------------------------------------------
# Find Geant4 package
# Set WITH_GEANT4_UIVIS=OFF for batch-only builds
#
option ( WITH_GEANT4_UIVIS "Build with Geant4 UI and Vis drivers" ON )

if ( WITH_GEANT4_UIVIS )
  find_package ( Geant4 REQUIRED ui_all vis_all )
else ()
  find_package ( Geant4 REQUIRED )
endif ()

# ----------------------------------------------------------------------------
# Geant4 setup + project include directory
#
include ( ${Geant4_USE_FILE} )
include_directories (
  ${PROJECT_SOURCE_DIR}/src/user
  ${PROJECT_SOURCE_DIR}/src/phys
  ${PROJECT_SOURCE_DIR}/src/action
  ${PROJECT_SOURCE_DIR}/src/edm
  ${PROJECT_SOURCE_DIR}/src/pmt
  ${PROJECT_SOURCE_DIR}/src/gen
)

# ----------------------------------------------------------------------------
# Find ROOT
#
find_package ( ROOT REQUIRED COMPONENTS Core Hist RIO Tree )
include ( ${ROOT_USE_FILE} )

# ----------------------------------------------------------------------------
# Find yaml-cpp
# Used only to read spectrum data tables that are too large to pass as UI
# command arguments (e.g. the IBD antineutrino spectrum). Run configuration
# itself stays in Geant4 macros.
#
find_package ( yaml-cpp REQUIRED )

# ----------------------------------------------------------------------------
# Sources and headers
#
# Only the detector itself stays in the executable.
# Everything a new experiment supplies: the geometry, the application's main, and
# any StepObserver of its own. The glob picks up main too, so it is not named
# separately below.
file ( GLOB user_sources ${PROJECT_SOURCE_DIR}/src/user/*.cc )

# ----------------------------------------------------------------------------
# SimGeneral — everything except the geometry
#
# Primary generators, physics list, user actions, PMT components and the ROOT
# output manager. None of it includes anything from src/user: generators name
# volumes as strings, and SteppingAction takes its region tagging as a callback
# the detector supplies. So another experiment provides a geometry and a main,
# links this, and reuses the rest.
#
# RootManager lives in src/action, not src/edm: it is the output side of the user
# actions and needs Geant4, whereas src/edm is the plain ROOT data model that
# analysis code reads without Geant4. Keeping that line clean also breaks what was
# a pmt <-> data include cycle, leaving src/edm a leaf.
#
file ( GLOB general_sources
  ${PROJECT_SOURCE_DIR}/src/gen/*.cc
  ${PROJECT_SOURCE_DIR}/src/phys/*.cc
  ${PROJECT_SOURCE_DIR}/src/action/*.cc
  ${PROJECT_SOURCE_DIR}/src/pmt/*.cc
)
file ( GLOB general_headers
  ${PROJECT_SOURCE_DIR}/src/gen/*.hh
  ${PROJECT_SOURCE_DIR}/src/phys/*.hh
  ${PROJECT_SOURCE_DIR}/src/action/*.hh
  ${PROJECT_SOURCE_DIR}/src/pmt/*.hh
)

add_library ( SimGeneral SHARED ${general_sources} )
target_include_directories ( SimGeneral PUBLIC
  ${PROJECT_SOURCE_DIR}/src/gen
  ${PROJECT_SOURCE_DIR}/src/phys
  ${PROJECT_SOURCE_DIR}/src/action
  ${PROJECT_SOURCE_DIR}/src/pmt
  ${PROJECT_SOURCE_DIR}/src/edm
)
# Where the installed data files end up, so ResolveDataFile has a last resort.
target_compile_definitions ( SimGeneral PRIVATE
  MUONSIM_DATADIR="${CMAKE_INSTALL_PREFIX}/data" )
target_link_libraries ( SimGeneral
  ${Geant4_LIBRARIES} ROOT::Core ROOT::Hist ROOT::RIO ROOT::Tree SimData yaml-cpp )
file ( GLOB headers
  ${PROJECT_SOURCE_DIR}/src/user/*.hh
  ${PROJECT_SOURCE_DIR}/src/phys/*.hh
  ${PROJECT_SOURCE_DIR}/src/action/*.hh
  ${PROJECT_SOURCE_DIR}/src/pmt/*.hh
  ${PROJECT_SOURCE_DIR}/src/gen/*.hh
)

# ----------------------------------------------------------------------------
# Standalone data library — loadable from ROOT macros without Geant4
#
set ( data_sources
  ${PROJECT_SOURCE_DIR}/src/edm/MCEventInfo.cc
  ${PROJECT_SOURCE_DIR}/src/edm/MCPhotonHit.cc
  ${PROJECT_SOURCE_DIR}/src/edm/MCPMT.cc
  ${PROJECT_SOURCE_DIR}/src/edm/MCPMTData.cc
  ${PROJECT_SOURCE_DIR}/src/edm/MCPrimary.cc
  ${PROJECT_SOURCE_DIR}/src/edm/MCPrimaryData.cc
  ${PROJECT_SOURCE_DIR}/src/edm/MCScint.cc
  ${PROJECT_SOURCE_DIR}/src/edm/MCScintStep.cc
  ${PROJECT_SOURCE_DIR}/src/edm/MCScintData.cc
  ${PROJECT_SOURCE_DIR}/src/edm/MCStep.cc
  ${PROJECT_SOURCE_DIR}/src/edm/MCTrack.cc
  ${PROJECT_SOURCE_DIR}/src/edm/MCTrackData.cc
)

# MODULE SimData points the generated rootmap at libSimData.so, which is where
# the dictionary actually ends up. Without it ROOT writes a rootmap naming
# libSimDataDict.so -- a library that is never built -- so autoloading fails and
# every analysis macro has to load the library by hand.
ROOT_GENERATE_DICTIONARY ( SimDataDict
  MCEventInfo.hh MCPhotonHit.hh MCPMT.hh MCPMTData.hh MCPrimary.hh MCPrimaryData.hh
  MCScintStep.hh MCScint.hh MCScintData.hh MCStep.hh MCTrack.hh MCTrackData.hh
  LINKDEF src/edm/LinkDef.h
  MODULE SimData
  OPTIONS -I${PROJECT_SOURCE_DIR}/src/edm
)

add_library ( SimData SHARED SimDataDict.cxx ${data_sources} )
target_include_directories ( SimData PUBLIC
  ${PROJECT_SOURCE_DIR}/src/user
  ${PROJECT_SOURCE_DIR}/src/phys
  ${PROJECT_SOURCE_DIR}/src/action
  ${PROJECT_SOURCE_DIR}/src/edm
  ${PROJECT_SOURCE_DIR}/src/pmt
  ${PROJECT_SOURCE_DIR}/src/gen
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries ( SimData ROOT::Core ROOT::RIO ROOT::Tree )

# ----------------------------------------------------------------------------
# Executable
#
add_executable ( MuonSim ${user_sources} )
target_include_directories ( MuonSim PRIVATE
  ${PROJECT_SOURCE_DIR}/src/user
  ${PROJECT_SOURCE_DIR}/src/phys
  ${PROJECT_SOURCE_DIR}/src/action
  ${PROJECT_SOURCE_DIR}/src/edm
  ${PROJECT_SOURCE_DIR}/src/pmt
  ${PROJECT_SOURCE_DIR}/src/gen
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries ( MuonSim ${Geant4_LIBRARIES} SimGeneral SimData )

# ----------------------------------------------------------------------------
# Install
#
include ( GNUInstallDirs )

configure_file ( setup_muonsim.sh.in setup_muonsim.sh @ONLY )
install ( PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/setup_muonsim.sh
  DESTINATION .
)

install ( TARGETS MuonSim
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install ( TARGETS SimData SimGeneral
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Headers, so another detector can link SimGeneral and reuse it
install ( FILES ${general_headers}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# ROOT dictionary support files (needed for TClass lookup)
install ( FILES
  ${CMAKE_CURRENT_BINARY_DIR}/libSimData.rootmap
  ${CMAKE_CURRENT_BINARY_DIR}/libSimData_rdict.pcm
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Data class headers (for use from ROOT macros without Geant4)
install ( FILES
  src/edm/MCEventInfo.hh
  src/edm/MCPhotonHit.hh
  src/edm/MCPMT.hh
  src/edm/MCPMTData.hh
  src/edm/MCPrimary.hh
  src/edm/MCPrimaryData.hh
  src/edm/MCScint.hh
  src/edm/MCScintData.hh
  src/edm/MCScintStep.hh
  src/edm/MCStep.hh
  src/edm/MCTrack.hh
  src/edm/MCTrackData.hh
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# ----------------------------------------------------------------------------
# Tests — ctest
#
enable_testing ()
add_subdirectory ( test )

# ----------------------------------------------------------------------------
# Macros and data directories
install ( DIRECTORY macro data
  DESTINATION .
)
