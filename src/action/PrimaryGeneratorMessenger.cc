#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIdirectory.hh"
#include "AbsVertexGen.hh" // complete type needed to destroy the unique_ptr we forward
#include "PrimaryGeneratorAction.hh"
#include "PrimaryGeneratorMessenger.hh"
#include "VertexGenBuilder.hh"

PrimaryGeneratorMessenger::PrimaryGeneratorMessenger(PrimaryGeneratorAction * action)
  : fAction(action)
{
  fDir = new G4UIdirectory("/gen/");
  fDir->SetGuidance("Primary vertex configuration.");

  fVertexCmd = new G4UIcmdWithAString("/gen/vertex", this);
  fVertexCmd->SetGuidance("Add one primary vertex. All vertices fire together in every event.");
  fVertexCmd->SetGuidance("");
  fVertexCmd->SetGuidance("  /gen/vertex <type> [<energy>] [clause] ...");
  fVertexCmd->SetGuidance("");
  fVertexCmd->SetGuidance("type -- a particle, an isotope, or a source:");
  fVertexCmd->SetGuidance("  e-, e+, mu-, gamma, alpha, neutron, ...   any Geant4 particle name");
  fVertexCmd->SetGuidance("  60Co, 232Th, 238U, 252Cf                  an isotope (Co60 spelling too);");
  fVertexCmd->SetGuidance("                                            leave the energy off so it");
  fVertexCmd->SetGuidance("                                            decays at rest");
  fVertexCmd->SetGuidance("  cosmic     muons from a flux histogram; derives its own position.");
  fVertexCmd->SetGuidance("             REQUIRES input <flux.root>: the SPHERE file, holding the");
  fVertexCmd->SetGuidance("             TH3D h_flux. It is launched from a sphere, so the PLANE file");
  fVertexCmd->SetGuidance("             would be wrong by 32% with nothing to notice. It sets the");
  fVertexCmd->SetGuidance("             clock itself, so the run reports a live time");
  fVertexCmd->SetGuidance("             (see 'sphere' below and doc/generator.md)");
  fVertexCmd->SetGuidance("  rockgamma  gammas from the rock, cos-weighted through the surface;");
  fVertexCmd->SetGuidance("             REQUIRES input <spectrum.yml>");
  fVertexCmd->SetGuidance("  IBD        inverse beta decay -> correlated e+ and neutron;");
  fVertexCmd->SetGuidance("             REQUIRES input <spectrum.yml> -- no standard spectrum exists");
  fVertexCmd->SetGuidance("  AmBe       neutron + 4.44 MeV gamma; built-in measured spectrum,");
  fVertexCmd->SetGuidance("             override it with an optional input <spectrum.yml>");
  fVertexCmd->SetGuidance("  252CfSF    one spontaneous fission: ~3.8 neutrons, ~7 gammas");
  fVertexCmd->SetGuidance("  file       ready-made events from a text file, for output of a");
  fVertexCmd->SetGuidance("             generator this code does not contain (DECAY0 and the like);");
  fVertexCmd->SetGuidance("             REQUIRES input <events.txt>. The file gives the particles,");
  fVertexCmd->SetGuidance("             the position clause says where -- so one file serves any");
  fVertexCmd->SetGuidance("             volume. See doc/generator.md for the format.");
  fVertexCmd->SetGuidance("  (a source sets its own energies, so it takes no energy spec)");
  fVertexCmd->SetGuidance("");
  fVertexCmd->SetGuidance("energy -- directly after the type; omit for 0 = at rest. Unit defaults");
  fVertexCmd->SetGuidance("          to MeV:");
  fVertexCmd->SetGuidance("  <value> [unit] | uniform <min> <max> | gauss <mean> <sigma>");
  fVertexCmd->SetGuidance("  | exp <E0> | powerlaw <index> <min> <max> | spectrum <E>:<w> ...");
  fVertexCmd->SetGuidance("");
  fVertexCmd->SetGuidance("clauses -- any order:");
  fVertexCmd->SetGuidance("  point <x> <y> <z> [unit]        fixed position (default mm)");
  fVertexCmd->SetGuidance("  involume <volume>               uniform inside it");
  fVertexCmd->SetGuidance("  onvolume [in|out|both] <volume> on its surface; in/out restricts the");
  fVertexCmd->SetGuidance("                                  emission to that side of the normal");
  fVertexCmd->SetGuidance("  multivolume <pattern>           volumes matching it, by cubic volume");
  fVertexCmd->SetGuidance("  random                          isotropic direction (the default)");
  fVertexCmd->SetGuidance("  sphere <x> <y> <z> <R> [unit]   'cosmic' only: the virtual surface muons");
  fVertexCmd->SetGuidance("                                  are launched from. R only has to enclose");
  fVertexCmd->SetGuidance("                                  what is being asked about; the answer does");
  fVertexCmd->SetGuidance("                                  not depend on it and the cost goes as R^2");
  fVertexCmd->SetGuidance("  rotate                          'file' only: turn each event to a random");
  fVertexCmd->SetGuidance("                                  orientation as it is used, which is what");
  fVertexCmd->SetGuidance("                                  makes replaying a file bigger than itself");
  fVertexCmd->SetGuidance("                                  worth anything");
  fVertexCmd->SetGuidance("  direction <dx> <dy> <dz>        fixed direction");
  fVertexCmd->SetGuidance("  cos | iso                       how a restricted surface spreads its");
  fVertexCmd->SetGuidance("                                  emission: Lambert, or uniform in solid");
  fVertexCmd->SetGuidance("                                  angle. cos is rockgamma's default");
  fVertexCmd->SetGuidance("  polarization <px> <py> <pz>");
  fVertexCmd->SetGuidance("  time <t> [unit]                 vertex time (default ns)");
  fVertexCmd->SetGuidance("  input <file>                    spectrum file. YAML for every source");
  fVertexCmd->SetGuidance("                                  except cosmic, which takes a .root TH3D");
  fVertexCmd->SetGuidance("");
  fVertexCmd->SetGuidance("A vertex outside the world aborts the run reporting the offending");
  fVertexCmd->SetGuidance("coordinates, rather than crashing inside the navigator. Use");
  fVertexCmd->SetGuidance("/control/manual or your geometry's documentation for volume names and");
  fVertexCmd->SetGuidance("the coordinate range it occupies.");
  fVertexCmd->SetGuidance("");
  fVertexCmd->SetGuidance("Examples (see the run macro for ones using this detector's volumes):");
  fVertexCmd->SetGuidance("  /gen/vertex e- 2.5 MeV point <x> <y> <z> mm direction 0 0 1");
  fVertexCmd->SetGuidance("  /gen/vertex e- uniform 0 3 MeV involume <volume> random");
  fVertexCmd->SetGuidance("  /gen/vertex 60Co involume <volume>");
  fVertexCmd->SetGuidance("  /gen/vertex gamma 1.46 MeV multivolume <namePattern>");
  fVertexCmd->SetGuidance("  /gen/vertex neutron 1 MeV onvolume in <volume>");
  fVertexCmd->SetGuidance("  /gen/vertex rockgamma onvolume in World input <spectrum.yml>");
  fVertexCmd->SetGuidance("  /gen/vertex IBD involume <volume> input <spectrum.yml>");
  fVertexCmd->SetGuidance("  /gen/vertex AmBe point <x> <y> <z> mm");
  fVertexCmd->SetGuidance("  /gen/vertex AmBe involume <volume> input <spectrum.yml>");
  fVertexCmd->SetGuidance("  /gen/vertex 252CfSF involume <volume>");
  fVertexCmd->SetGuidance("  /gen/vertex cosmic input <flux.root>");

  fClearCmd = new G4UIcmdWithoutParameter("/gen/clear", this);
  fClearCmd->SetGuidance("Remove all vertices. At least one must be defined before /run/beamOn.");

  fListCmd = new G4UIcmdWithoutParameter("/gen/list", this);
  fListCmd->SetGuidance("Print the currently defined vertices.");

  fYieldCmd = new G4UIcmdWithABool("/gen/yieldToDeferred", this);
  fYieldCmd->SetGuidance("Plant nothing in an event that inherited deferred tracks.");
  fYieldCmd->SetGuidance("");
  fYieldCmd->SetGuidance("Goes with the eventwindow stack observer, which splits a decay chain");
  fYieldCmd->SetGuidance("across events: without this, an event holds both the tail of the last");
  fYieldCmd->SetGuidance("chain and a brand new nucleus, and the two are indistinguishable");
  fYieldCmd->SetGuidance("afterwards. The event count then stops being the number of decays --");
  fYieldCmd->SetGuidance("normalise with the primaries generated instead.");
  fYieldCmd->SetParameterName("yield", true);
  fYieldCmd->SetDefaultValue(true);

  fActivityCmd = new G4UIcmdWithADoubleAndUnit("/gen/activity", this);
  fActivityCmd->SetGuidance("Activity of the source, which drives the universal clock.");
  fActivityCmd->SetGuidance("");
  fActivityCmd->SetGuidance("Each nucleus is planted a Poisson gap after the last, drawn at this");
  fActivityCmd->SetGuidance("rate, so the clock at the end of the run is the LIVE TIME it covers");
  fActivityCmd->SetGuidance("and a count rate is counts divided by that. It is written into every");
  fActivityCmd->SetGuidance("event as MCEventInfo::GetUniversalTime(), in seconds.");
  fActivityCmd->SetGuidance("");
  fActivityCmd->SetGuidance("The default 1 Bq makes the gaps average a second, so live time in");
  fActivityCmd->SetGuidance("seconds equals the number of decays -- dividing by it is then exactly");
  fActivityCmd->SetGuidance("the per-decay normalisation, and setting a real activity turns the");
  fActivityCmd->SetGuidance("same division into a real rate.");
  fActivityCmd->SetGuidance("");
  fActivityCmd->SetGuidance("IT DOES NOT GIVE PILEUP. Two decays landing in one window needs a");
  fActivityCmd->SetGuidance("queue that merges them, which does not exist here: with");
  fActivityCmd->SetGuidance("/gen/yieldToDeferred on, exactly one chain is ever in flight.");
  fActivityCmd->SetParameterName("activity", true);
  fActivityCmd->SetDefaultValue(1.);
  fActivityCmd->SetDefaultUnit("Bq");

  fWindowCmd = new G4UIcmdWithADoubleAndUnit("/gen/window", this);
  fWindowCmd->SetGuidance("How long one event lasts -- the detector's integration time.");
  fWindowCmd->SetGuidance("");
  fWindowCmd->SetGuidance("Two things read it. Decays whose Poisson gap falls inside it are planted");
  fWindowCmd->SetGuidance("in the SAME event, which is pileup; and the eventwindow stack observer");
  fWindowCmd->SetGuidance("defers anything in a decay chain arriving later than it, which is a");
  fWindowCmd->SetGuidance("chain being split. Both are the same boundary, so there is one command.");
  fWindowCmd->SetGuidance("");
  fWindowCmd->SetGuidance("Match it to the readout: microseconds for a scintillator, milliseconds");
  fWindowCmd->SetGuidance("for a cryogenic calorimeter -- and pileup goes as activity times this.");
  fWindowCmd->SetParameterName("window", true);
  fWindowCmd->SetDefaultValue(1.);
  fWindowCmd->SetDefaultUnit("us");
}


PrimaryGeneratorMessenger::~PrimaryGeneratorMessenger()
{
  delete fVertexCmd;
  delete fClearCmd;
  delete fListCmd;
  delete fYieldCmd;
  delete fActivityCmd;
  delete fWindowCmd;
  delete fDir;
}

void PrimaryGeneratorMessenger::SetNewValue(G4UIcommand * command, G4String newValue)
{
  if (command == fVertexCmd) { fAction->AddVertex(BuildVertexGen(newValue)); }
  else if (command == fClearCmd) {
    fAction->ClearVertices();
    G4cout << "/gen/clear: all vertices removed -- define one before /run/beamOn." << G4endl;
  }
  else if (command == fWindowCmd) {
    fAction->SetEventWindow(fWindowCmd->GetNewDoubleValue(newValue));
  }
  else if (command == fActivityCmd) {
    fAction->SetActivity(fActivityCmd->GetNewDoubleValue(newValue));
  }
  else if (command == fYieldCmd) {
    fAction->SetYieldToDeferred(fYieldCmd->GetNewBoolValue(newValue));
  }
  else if (command == fListCmd) {
    fAction->ListVertices();
  }
}
