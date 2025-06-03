#ifndef SteppingAction_h
#define SteppingAction_h 1

#include "G4UserSteppingAction.hh"
#include "globals.hh"

class MyTrackInfo;  // Forward declaration of your custom TrackInfo
class DetectorConstruction;
class EventAction;

class SteppingAction : public G4UserSteppingAction
{
public:
    SteppingAction() = default;
    virtual ~SteppingAction() = default;

    virtual void UserSteppingAction(const G4Step* step) override;

private:
    // Optionally add pointers to other classes if needed (e.g., EventAction*)
};

#endif
