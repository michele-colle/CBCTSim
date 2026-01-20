#include <detector.hh>
#include <G4Gamma.hh>
#include <TrackInfo.hh>
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

void SensitiveDetector::Initialize(G4HCofThisEvent *hce)
{
  // Create a new hits collection for this event
  fHitsCollection = new MyHitsCollection(SensitiveDetectorName, collectionName[0]);
  auto name = collectionName[0];
  // Add this collection to the Geant4 Hits Collection of This Event (HCE)
  // We get a unique ID for it that we can use to retrieve it later.
  G4int hcID = G4SDManager::GetSDMpointer()->GetCollectionID(collectionName[0]);
  hce->AddHitsCollection(hcID, fHitsCollection);
}

G4bool SensitiveDetector::ProcessHits(G4Step *aStep, G4TouchableHistory *ROhist)
{
  // G4cout << "SensitiveDetector::ProcessHits called" << G4endl;

  // 1. Get the EVENT-GLOBAL information (the Event ID)
  // qua sembrano parlare di come discriminare lo scatter
  // https://www.researchgate.net/post/How-to-stop-particles-tracking-in-GEANT4
  G4Track *track = aStep->GetTrack();
  // questo impedisce la generazione di fotoni nel materiale del detector (glare?)
  track->SetTrackStatus(fStopAndKill);
  if (track->GetDefinition() != G4Gamma::Definition())
    return false;

  // Retrieve our minimal TrackInfo
  TrackInfo* info = static_cast<TrackInfo*>(track->GetUserInformation());

  G4String procType = "Unknown";

  if (info) {
      // 1. PRIMARY: No parent AND the isScattered flag is still false
      if (track->GetParentID() == 0 && !info->isScattered) {
          procType = "Primary";
      }
      // 2. SCATTERED: No parent (it's the original track) but flag is true
      else if (track->GetParentID() == 0 && info->isScattered) {
          procType = "Scatter";
      }
      // 3. SECONDARY: Has a parent (Fluorescence, etc.)
      else if (track->GetParentID() > 0) {
          procType = "Secondary"; 
          // Optional: if (info->isFluo) procType = "Fluorescence";
      }
  }

  G4StepPoint *preStepPoint = aStep->GetPreStepPoint();
  MyHit *newHit = new MyHit();

  newHit->SetEnergy(preStepPoint->GetKineticEnergy());
  newHit->SetPosition(preStepPoint->GetPosition());
  newHit->SetMomentum(preStepPoint->GetMomentum());
  newHit->SetProcess(procType);

  // Add the new hit to our collection for this event
  fHitsCollection->insert(newHit);

  return true;
}