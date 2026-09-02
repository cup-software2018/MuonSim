#include "G4Gamma.hh"
#include "G4Neutron.hh"
#include "G4ParticleDefinition.hh"
#include "G4Track.hh"
#include "G4ios.hh"

#include "ObserverRegistry.hh"
#include "RockGammaStackingObserver.hh"

RockGammaStackingObserver::RockGammaStackingObserver()
  : StackingObserver("rockgamma")
{
}

namespace {

// Announces itself, so main names nothing and adding an observer is adding a file.
const bool registered = ObserverRegistry<StackingObserver>::Add(
    "rockgamma", [] { return std::make_shared<RockGammaStackingObserver>(); });

} // namespace

std::string RockGammaStackingObserver::Describe() const
{
  return "keeps gammas and the nuclei that emit them, kills every other secondary";
}

bool RockGammaStackingObserver::Classify(const G4Track * track,
                                      G4ClassificationOfNewTrack & classification)
{
  // The primary is the radioactive nucleus itself. Kill it and nothing decays.
  if (track->GetParentID() == 0) {
    fKeptPrimaries++;
    return false;
  }

  const G4ParticleDefinition * particle = track->GetDefinition();
  if (!particle) return false;

  if (particle == G4Gamma::Definition()) {
    fKeptGammas++;
    return false;
  }

  // The daughter nucleus, which is what actually emits the decay gamma, and in a
  // chain is also the next link. Alphas are ions too and come along with it.
  if (particle->GetAtomicNumber() > 0) {
    fKeptIons++;
    return false;
  }

  if (particle == G4Neutron::Definition())
    fKilledNeutrons++;
  else
    fKilledOther++;

  classification = fKill;
  return true;
}

void RockGammaStackingObserver::EndOfRun(const G4Run *, RootManager *)
{
  const long long kept = fKeptPrimaries + fKeptGammas + fKeptIons;
  const long long killed = fKilledNeutrons + fKilledOther;
  const long long total = kept + killed;
  if (total == 0) return;

  G4cout << "RockGammaStackingObserver: " << total << " tracks offered, " << killed << " killed ("
         << 100. * killed / total << "%) -- kept " << fKeptPrimaries << " primaries, "
         << fKeptGammas << " gammas, " << fKeptIons << " ions" << G4endl;

  // Not an error, but it is the one loss that is not bremsstrahlung below 300 keV:
  // no neutron means no capture gamma. Said out loud so it is not found later.
  if (fKilledNeutrons > 0)
    G4cout << "RockGammaStackingObserver: " << fKilledNeutrons
           << " neutrons killed -- their capture gammas are NOT in this Escape tree" << G4endl;

  fKeptPrimaries = fKeptGammas = fKeptIons = fKilledNeutrons = fKilledOther = 0;
}
