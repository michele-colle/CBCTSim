#ifndef DETECTOR_HH
#define DETECTOR_HH

#include <G4VSensitiveDetector.hh>
#include <G4AnalysisManager.hh>
#include <G4RunManager.hh>
#include <G4SystemOfUnits.hh>
//#include <MyHit.hh>


class SensitiveDetector : public G4VSensitiveDetector
{
public:
    SensitiveDetector(G4String);
    ~SensitiveDetector();
    void Initialize(G4HCofThisEvent*) override;

private:
    virtual G4bool ProcessHits(G4Step *,G4TouchableHistory *);
    //MyHitsCollection* fHitsCollection;
};

#endif