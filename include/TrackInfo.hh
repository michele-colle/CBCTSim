#ifndef TRACKINFO_HH
#define TRACKINFO_HH

#include "G4VUserTrackInformation.hh"
#include "globals.hh"

class TrackInfo : public G4VUserTrackInformation {
public:
    TrackInfo() : isScattered(false), isFluo(false) {}
    virtual ~TrackInfo() {}

    // Minimal bool flags
    G4bool isScattered; 
    G4bool isFluo;

    // Necessary for propagation logic
    virtual void Print() const override {}
};

#endif