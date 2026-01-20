#include "SteppingAction.hh"
#include "TrackInfo.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4Gamma.hh"
#include "G4VProcess.hh"


void SteppingAction::UserSteppingAction(const G4Step* step) {
    G4Track* track = step->GetTrack();
    
    // 1. Performance: Only process Gammas (or skip if track is already dead)
    if (track->GetDefinition() != G4Gamma::Definition()) return;

    TrackInfo* info = static_cast<TrackInfo*>(track->GetUserInformation());
    if (!info) return;

    // 2. Identify the process that just happened at the PostStepPoint
    const G4VProcess* proc = step->GetPostStepPoint()->GetProcessDefinedStep();
    if (proc) {
        G4String pName = proc->GetProcessName();
        
        // Fast check: if it's a scatter, mark the track
        // 'compt' = Compton, 'Rayl' = Rayleigh
        if (pName == "compt" || pName == "Rayl") {
            info->isScattered = true;
        }
    }

    // 3. Propagate info to secondaries (Fluorescence photons)
    const std::vector<const G4Track*>* secondaries = step->GetSecondaryInCurrentStep();
    if (secondaries && secondaries->size() > 0) {
        for (auto& secTrack : *secondaries) {
            TrackInfo* newInfo = new TrackInfo();
            
            // Mark if this secondary is a fluorescence photon
            // Usually created by 'phot' (Photoelectric) or 'compt'
            if (secTrack->GetDefinition() == G4Gamma::Definition()) {
                newInfo->isFluo = true;
            }
            
            secTrack->SetUserInformation(newInfo);
        }
    }
}