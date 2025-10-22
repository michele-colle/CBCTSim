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
  //Create a new hits collection for this event
  fHitsCollection = new MyHitsCollection(SensitiveDetectorName, collectionName[0]);
  auto name = collectionName[0];
  // Add this collection to the Geant4 Hits Collection of This Event (HCE)
  // We get a unique ID for it that we can use to retrieve it later.
  G4int hcID = G4SDManager::GetSDMpointer()->GetCollectionID(collectionName[0]);
  hce->AddHitsCollection(hcID, fHitsCollection);
}

G4bool SensitiveDetector::ProcessHits(G4Step *aStep, G4TouchableHistory *ROhist)
{
  //G4cout << "SensitiveDetector::ProcessHits called" << G4endl;

    // 1. Get the EVENT-GLOBAL information (the Event ID)
  // qua sembrano parlare di come discriminare lo scatter
  // https://www.researchgate.net/post/How-to-stop-particles-tracking-in-GEANT4
  G4Track *track = aStep->GetTrack();
  // questo impedisce la generazione di fotoni nel materiale del detector (glare?)
  track->SetTrackStatus(fStopAndKill);
  if (track->GetDefinition() != G4Gamma::Definition())
    return false;

    // Create a new hit object

  // Get the process that defined/limited the current step
    const G4VProcess* processDefinedStep = 
        aStep->GetPostStepPoint()->GetProcessDefinedStep();
  G4String procType = "";

    // Check if the pointer is valid before using it
    if (processDefinedStep) {
        procType = processDefinedStep->GetProcessName();
    } 
  
  
  //if(proc == G4Process::fTransportation)
    //return false; // primary photon
  // // Get primary info from the TrackInfo user object
  // // const G4Event* currentEvent = G4RunManager::GetRunManager()->GetCurrentEvent();
  // // EventInfo* info = (EventInfo*)(currentEvent->GetUserInformation());
  // // if (info) {
  // //   newHit->SetPrimaryEnergy(info->primaryEnergy);
  // //   newHit->SetPrimaryMomentum(info->primaryMomentum);
  // // }
  
  // // Get hit info from the current step
   G4StepPoint* preStepPoint = aStep->GetPreStepPoint();
  MyHit* newHit = new MyHit();

  newHit->SetEnergy(preStepPoint->GetKineticEnergy());
  newHit->SetPosition(preStepPoint->GetPosition());
  newHit->SetMomentum(preStepPoint->GetMomentum());
  newHit->SetProcess(procType);

  // Add the new hit to our collection for this event
  fHitsCollection->insert(newHit);
    //G4cout << fHitsCollection->GetSize() << G4endl;

  //   G4AnalysisManager *analysis = G4AnalysisManager::Instance();
  // auto par = CBCTParams::Instance();
  // auto sourcePos = G4ThreeVector(0, -par->GetDSO(), 0);

  //   G4double en = preStepPoint->GetKineticEnergy();
  //   G4ThreeVector posPhoton = preStepPoint->GetPosition();
  //   G4ThreeVector momPhotonDirection = preStepPoint->GetMomentum().unit();
    

  //   G4ThreeVector p = (posPhoton - sourcePos).unit();
  //   G4double dot = momPhotonDirection.dot(p);
  //   //G4double E = primaryEnergy;
  //   const bool isCollinear = (dot >= 1.0 - DBL_EPSILON);
  //   //G4double deltaEnergy = E - en;

  //   // primary
  //   if (isCollinear)
  //   {
  //     analysis->FillH1(1, en);
  //     analysis->FillH2(1, posPhoton.x(), posPhoton.z(), en / keV);
  //   }
  //   else // scatter
  //   {
  //     analysis->FillH1(2, en);
  //     analysis->FillH2(2, posPhoton.x(), posPhoton.z(), en / keV);
  //   }

  return true;

}