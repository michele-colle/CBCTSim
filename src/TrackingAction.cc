#include "G4TrackingManager.hh"
#include "G4RunManager.hh"
#include "EventAction.hh"
#include "G4Track.hh"
#include "TrackInfo.hh"  // Include your custom track info class
#include "TrackingAction.hh"  // Include your custom track info class

// In TrackingAction.cc
void TrackingAction::PreUserTrackingAction(const G4Track* track) {
    // Give every new track a TrackInfo object
    if (!track->GetUserInformation()) {
        G4EventManager::GetEventManager()->GetTrackingManager()->SetUserTrackInformation(new TrackInfo());
    }
}
