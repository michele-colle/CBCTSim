#include "G4TrackingManager.hh"
#include "G4RunManager.hh"
#include "EventAction.hh"
#include "G4Track.hh"
#include "TrackInfo.hh"  // Include your custom track info class
#include "TrackingAction.hh"  // Include your custom track info class

void TrackingAction::PreUserTrackingAction(const G4Track* track) {
    G4int parentID = track->GetParentID();
    G4int trackID = track->GetTrackID();

    if (parentID == 0) {
        // It's a primary particle
        G4ThreeVector momentum = track->GetMomentum();
        G4double energy = track->GetTotalEnergy();

        auto info = new TrackInfo(momentum, energy, trackID);
        const_cast<G4Track*>(track)->SetUserInformation(info);
    }
    else
    {
        EventAction* ev = (EventAction*)(G4RunManager::GetRunManager()->GetUserEventAction());
        ev->IncrementSecondaryParticleCounter();
    }
}
