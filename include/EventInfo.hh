#ifndef EventInfo_h
#define EventInfo_h 1
#include "G4VUserEventInformation.hh"
#include "globals.hh"
#include "G4ThreeVector.hh"


class EventInfo : public G4VUserEventInformation
{
    public:
    G4ThreeVector primaryMomentum;
    G4double primaryEnergy;
    G4int primaryID;

    //is called a member initializer list. It initializes the member variables before the constructor body runs.
    EventInfo(G4ThreeVector mom, G4double energy, G4int pid)
        : primaryMomentum(mom), primaryEnergy(energy), primaryID(pid) {}

    virtual ~EventInfo() = default;
    virtual void Print() const override {}
};
#endif