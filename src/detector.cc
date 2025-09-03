#include <detector.hh>
#include <G4Gamma.hh>
//#include <TrackInfo.hh>
#include <TxtWithHeaderReader.hh>
#include <CBCTParams.hh>
#include <EventInfo.hh>
#include <MyHit.hh>
#include <G4SDManager.hh>

SensitiveDetector::SensitiveDetector(G4String name) : G4VSensitiveDetector(name)
{
  collectionName.insert("MyHitsCollection");

}
SensitiveDetector::~SensitiveDetector() {}

void SensitiveDetector::Initialize(G4HCofThisEvent* hce)
{
  // Create a new hits collection for this event
  fHitsCollection = new MyHitsCollection(SensitiveDetectorName, collectionName[0]);

  // Add this collection to the Geant4 Hits Collection of This Event (HCE)
  // We get a unique ID for it that we can use to retrieve it later.
  G4int hcID = G4SDManager::GetSDMpointer()->GetCollectionID(collectionName[0]);
  hce->AddHitsCollection(hcID, fHitsCollection);
}

G4bool SensitiveDetector::ProcessHits(G4Step *aStep, G4TouchableHistory *ROhist)
{

    // 1. Get the EVENT-GLOBAL information (the Event ID)
  // qua sembrano parlare di come discriminare lo scatter
  // https://www.researchgate.net/post/How-to-stop-particles-tracking-in-GEANT4
  G4Track *track = aStep->GetTrack();
  // questo impedisce la generazione di fotoni nel materiale del detector (glare?)
  track->SetTrackStatus(fStopAndKill);
  if (track->GetDefinition() != G4Gamma::Definition())
    return false;

    // Create a new hit object
  MyHit* newHit = new MyHit();

  // Get primary info from the TrackInfo user object
  const G4Event* currentEvent = G4RunManager::GetRunManager()->GetCurrentEvent();
  EventInfo* info = (EventInfo*)(currentEvent->GetUserInformation());
  if (info) {
    newHit->SetPrimaryEnergy(info->primaryEnergy);
    newHit->SetPrimaryMomentum(info->primaryMomentum);
  }
  
  // Get hit info from the current step
  G4StepPoint* preStepPoint = aStep->GetPreStepPoint();
  newHit->SetEnergy(preStepPoint->GetKineticEnergy());
  newHit->SetPosition(preStepPoint->GetPosition());
  newHit->SetMomentum(preStepPoint->GetMomentum());

  // Add the new hit to our collection for this event
  fHitsCollection->insert(newHit);
  
  return true;

}