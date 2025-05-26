// #ifndef TRACKINFO_HH
// #define TRACKINFO_HH

// #include "G4VUserTrackInformation.hh"
// #include "globals.hh"
// #include "G4ThreeVector.hh"

// class TrackInfo : public G4VUserTrackInformation 
// {
// public:
//     G4ThreeVector primaryMomentum;
//     G4double primaryEnergy;
//     G4int primaryID;

//     //is called a member initializer list. It initializes the member variables before the constructor body runs.
//     TrackInfo(G4ThreeVector mom, G4double energy, G4int pid)
//         : primaryMomentum(mom), primaryEnergy(energy), primaryID(pid) {}

//     virtual ~TrackInfo() = default;
//     virtual void Print() const override {}
// };

// #endif
