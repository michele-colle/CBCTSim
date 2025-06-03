#include<SteppingAction.hh>
#include<TrackInfo.hh>
#include<G4Track.hh>
#include<G4Step.hh>
void SteppingAction::UserSteppingAction(const G4Step* step) {
    const G4Track* parentTrack = step->GetTrack();
    auto* parentInfo = dynamic_cast<TrackInfo*>(parentTrack->GetUserInformation());

    if (!parentInfo) return; // if parent has no info, nothing to propagate

    // Get secondaries produced in this step
    const auto* secondaries = step->GetSecondaryInCurrentStep();
    for (auto* secondary : *secondaries) {
        auto* newInfo = new TrackInfo(parentInfo->primaryMomentum, parentInfo->primaryEnergy, parentInfo->primaryID);
        const_cast<G4Track*>(secondary)->SetUserInformation(newInfo);
    }
}
