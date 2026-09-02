#include <chrono>
#include <cstdlib>
#include <getopt.h>
#include <memory>
#include <string>
#include <vector>

#include "ActionInitialization.hh"
#include "DetectorConstruction.hh"
#include "G4RunManagerFactory.hh"
#include "G4SteppingVerbose.hh"
#include "G4UIExecutive.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"
#include "MaterialPropertyFile.hh"
#include "Randomize.hh"
#include "EventObserver.hh"
#include "RunObserver.hh"
#include "ObserverRegistry.hh"
#include "StackingObserver.hh"
#include "SteppingObserver.hh"
#include "TrackingObserver.hh"
#include "UndergroundPhysicsList.hh"

namespace
{

// CLHEP takes a seed in [1, 900000000].
constexpr long kMaxSeed = 900000000L;

// What {nevent} expands to when -n is not given, so a macro written as
// /run/beamOn {nevent} still runs standalone.
constexpr long kDefaultNEvent = 10000L;

void Usage(const char * prog)
{
  G4cout << "Usage: " << prog
         << " [options] [macro]\n"
            "\n"
            "  -m, --material <file>  optical property YAML. Repeatable, each file adding to\n"
            "                         the ones before. With none given, the file shipped with\n"
            "                         the installation is used.\n"
            "  -g, --geometry <file>  geometry YAML. RESERVED -- no loader exists yet, and a\n"
            "                         run started with one refuses rather than ignoring it.\n"
            "  -o, --output <file>    output ROOT file  [muon_output.root]\n"
            "  -n, --nevent <n>       events to generate ["
         << kDefaultNEvent
         << "]. Reaches the macro as the\n"
            "                         alias {nevent}, so a macro fires /run/beamOn {nevent};\n"
            "                         one that hardcodes a count is unaffected.\n"
            "  -s, --seed <n>         random seed, 1.."
         << kMaxSeed
         << ". Omitted or 0 seeds from the\n"
            "                         clock, which is what parallel jobs want; give one to\n"
            "                         make a run reproducible byte for byte.\n"
            "  -h, --help             this text\n"
            "\n"
            "  macro                  the macro to run. With none, an interactive session\n"
            "                         starts and executes vis.mac.\n"
            "\n"
            "The primary generator takes no command-line options -- it is configured from the\n"
            "macro alone, with /gen/vertex. The cosmic muon flux file goes there too:\n"
            "\n"
            "  /gen/vertex cosmic input <flux.root>\n"
         << G4endl;
}

} // namespace

int main(int argc, char ** argv)
{
  G4String macroFile;
  G4String outputFile = "muon_output.root";
  G4String geometryFile;
  bool rockGamma = false;
  long seed = 0;
  long nevent = kDefaultNEvent;

  static const struct option longOptions[] = {
      {"material", required_argument, nullptr, 'm'}, {"geometry", required_argument, nullptr, 'g'},
      {"output", required_argument, nullptr, 'o'},   {"seed", required_argument, nullptr, 's'},
      {"nevent", required_argument, nullptr, 'n'},   {"rockgamma", no_argument, nullptr, 'r'},
      {"help", no_argument, nullptr, 'h'},           {nullptr, 0, nullptr, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, "m:g:o:s:n:h", longOptions, nullptr)) != -1) {
    switch (opt) {
      case 'm': RegisterMaterialPropertyFile(optarg); break;
      case 'g': geometryFile = optarg; break;
      case 'o': outputFile = optarg; break;
      case 's': {
        char * end = nullptr;
        seed = std::strtol(optarg, &end, 10);
        if (end == optarg || *end != '\0' || seed < 0 || seed > kMaxSeed) {
          G4cerr << "--seed takes an integer in 0.." << kMaxSeed << ", got '" << optarg << "'"
                 << G4endl;
          return 1;
        }
        break;
      }
      case 'n': {
        char * end = nullptr;
        nevent = std::strtol(optarg, &end, 10);
        if (end == optarg || *end != '\0' || nevent < 0) {
          G4cerr << "--nevent takes a non-negative integer, got '" << optarg << "'" << G4endl;
          return 1;
        }
        break;
      }
      case 'r': rockGamma = true; break;
      case 'h': Usage(argv[0]); return 0;
      default: // getopt_long has already said what was wrong
        Usage(argv[0]);
        return 1;
    }
  }

  // getopt_long permutes non-options to the end, so options may follow the macro
  // name as well as precede it.
  if (optind < argc) macroFile = argv[optind++];
  if (optind < argc) {
    G4cerr << "unexpected argument '" << argv[optind] << "' -- one macro at most" << G4endl;
    return 1;
  }

  // Only the program name reaches G4UIExecutive: it reads argv to pick a session
  // type, and would take our options for session hints.
  char * uiArgv[] = {argv[0], nullptr};
  G4UIExecutive * ui = macroFile.empty() ? new G4UIExecutive(1, uiArgv) : nullptr;

  // Seed from the high-resolution clock (microsecond precision) unless one was
  // given -- unique across parallel jobs, where a fixed seed would have every job
  // simulate the same events.
  if (seed == 0)
    seed = std::chrono::high_resolution_clock::now().time_since_epoch().count() % kMaxSeed;
  G4Random::setTheSeed(seed);
  G4cout << "Random seed: " << seed << G4endl;

  G4SteppingVerbose::UseBestUnit(4);

  auto * runManager = G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial);

  auto * detector = new DetectorConstruction();
  detector->SetGeometryFile(geometryFile);
  runManager->SetUserInitialization(detector);

  auto * physicsList = new UndergroundPhysicsList();
  physicsList->SetVerboseLevel(1);
  runManager->SetUserInitialization(physicsList);

  // Whatever observers this build contains, each of which registered itself. They
  // all start off; the macro says which ones this run uses, with
  // /observer/<family>/enable. An observer that kills tracks changes the physics, so
  // being linked in must not be the same as being asked for.
  auto * actions = new ActionInitialization(
      outputFile, ObserverRegistry<SteppingObserver>::CreateAll(),
      ObserverRegistry<TrackingObserver>::CreateAll(),
      ObserverRegistry<StackingObserver>::CreateAll(),
      ObserverRegistry<EventObserver>::CreateAll(), ObserverRegistry<RunObserver>::CreateAll());

  // Optional and independent -- drop either line if this geometry has nothing to
  // label that way. main knows both concrete types, which is why the taggers are
  // handed over here rather than cast back out of the run manager.
  actions->SetRegionTagger(detector->MakeRegionTagger());
  actions->SetDetectorIDTagger(detector->MakeDetectorIDTagger());

  runManager->SetUserInitialization(actions);

  auto * UImanager = G4UImanager::GetUIpointer();

  // The event count reaches the macro as an alias rather than as a beamOn issued
  // from here: a macro that already fires the beam would otherwise run twice, and
  // this way the macro stays the one place that says when the run starts.
  UImanager->ApplyCommand("/control/alias nevent " + std::to_string(nevent));

  if (ui) {
    auto * visManager = new G4VisExecutive;
    visManager->Initialize();
    UImanager->ApplyCommand("/control/execute vis.mac");
    ui->SessionStart();
    delete ui;
    delete visManager;
  }
  else {
    UImanager->ApplyCommand("/control/execute " + macroFile);
  }

  delete runManager;
}
