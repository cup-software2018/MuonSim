#pragma once

#include <memory>
#include <vector>

#include "G4String.hh"
#include "G4Types.hh"
#include "G4VUserPrimaryGeneratorAction.hh"

class AbsVertexGen;
class G4Event;
class PrimaryGeneratorMessenger;

/// Owns the primary vertex generators and fires them each event.
///
/// Vertices come from /gen/vertex, and there is no default: a run that defines
/// none is a configuration error, not a cosmic-muon run. There used to be an
/// implicit cosmic fallback reading a flux file named on the command line, but
/// /gen/vertex cosmic input <flux.root> says the same thing explicitly, and the
/// fallback's file could only ever be set from outside the macro.
///
/// Every vertex fires in the same event, each re-sampling its own position,
/// energy and direction.
class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
public:
  PrimaryGeneratorAction();
  ~PrimaryGeneratorAction() override;

  void GeneratePrimaries(G4Event *) override;

  // Defined out of line: AbsVertexGen is only forward-declared here.
  // Adopts the vertex's own rate for the clock, if it declares one. A cosmic
  // source does: the flux table fixes how many muons a second cross its launch
  // sphere, so asking the user for an activity would invite a number that
  // disagrees with the histogram. Radioactive sources declare nothing and keep
  // whatever /gen/activity says.
  void AddVertex(std::unique_ptr<AbsVertexGen> vertex);
  void ClearVertices();
  void ListVertices() const;
  std::size_t GetNumberOfVertices() const;

  // Skip generating while Geant4 still holds deferred tracks.
  //
  // For the decay-chain event window: a track deferred out of one event comes back
  // at the start of the next one, and if a fresh source nucleus is planted in that
  // same event then one event holds two unrelated chains. With this on, an event
  // that inherits deferred tracks gets nothing new and the chain finishes on its
  // own. Off by default -- every other kind of run wants a primary every event.
  void SetYieldToDeferred(G4bool yield) { fYieldToDeferred = yield; }
  G4bool GetYieldToDeferred() const { return fYieldToDeferred; }

  // Source nuclei actually planted, which stops being the event count as soon as
  // the above is on. Anything quoting a rate per decay needs THIS, not the number
  // of events.
  G4long GetPrimariesGenerated() const { return fPrimariesGenerated; }
  void ResetPrimariesGenerated() { fPrimariesGenerated = 0; }

  // The source's own clock. Each nucleus is planted a Poisson gap after the last,
  // drawn at this activity, so the clock at the end of a run is the live time the
  // run covers and a rate is counts divided by it.
  //
  // The default of 1 Bq makes the gaps average a second, so live time in seconds
  // equals the number of decays: dividing by it IS the per-decay normalisation this
  // code used before there was a clock. Set the real activity to get a real rate.
  //
  // It buys the bookkeeping, not pileup. Overlapping two decays in one event needs
  // the queue that merges them by time, and that does not exist yet -- with
  // yieldToDeferred on, exactly one chain is ever in flight.
  void SetActivity(G4double activity) { fActivity = activity; }
  G4double GetActivity() const { return fActivity; }

  // How long one event lasts. It is the generator's because the generator is what
  // decides which decays share an event; the stack observer reads it back so the
  // two halves -- grouping decays together and splitting a chain apart -- cannot
  // disagree about where the boundary is.
  void SetEventWindow(G4double window) { fEventWindow = window; }
  G4double GetEventWindow() const { return fEventWindow; }

  // Decays that landed on top of another one, over the run. Zero at 1 Bq and rising
  // with activity, which is the whole point: nothing special-cases pileup on or off.
  G4long GetPiledUp() const { return fPiledUp; }

  // In Geant4 time units. Divide by CLHEP::second for the live time in seconds.
  G4double GetUniversalTime() const { return fUniversalTime; }
  void ResetUniversalTime() { fUniversalTime = 0.; }

private:
  G4double NextGap();
  void PlantOne(G4Event * anEvent, G4double timeInEvent);

  std::unique_ptr<PrimaryGeneratorMessenger> fMessenger;
  std::vector<std::unique_ptr<AbsVertexGen>> fVertices;

  G4bool fYieldToDeferred = false;
  G4long fPrimariesGenerated = 0;

  G4double fActivity;        // set in the constructor, needs CLHEP units
  G4double fEventWindow;     // ditto
  G4double fUniversalTime = 0.;

  // Drawn but not yet planted, because it fell outside the last event's window.
  // Kept rather than redrawn: redrawing throws away the gap that was already
  // sampled and biases the interval distribution.
  G4double fNextDecayTime = -1.;
  G4long fPiledUp = 0;
};
