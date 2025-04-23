#include <detector.hh>
MySensitiveDetector::MySensitiveDetector(G4String name) : G4VSensitiveDetector(name)
{
    
}
MySensitiveDetector::~MySensitiveDetector(){}

G4bool MySensitiveDetector::ProcessHits(G4Step *aStep,G4TouchableHistory *ROhist)
{
    G4Track *track = aStep->GetTrack();
    //questo impedisce la generazione di fotoni nel materiale del detector (glare?)
    track->SetTrackStatus(fStopAndKill);
    G4StepPoint *preStepPoint = aStep->GetPreStepPoint();
    G4StepPoint *postStepPoint = aStep->GetPostStepPoint();

    G4ThreeVector posPhoton = preStepPoint->GetPosition();
    //usando questa funzione si possono stampare le posizioni di ingresso dei fotoni nel detector
    //G4cout << " Photon position: "<< posPhoton<<G4endl;
    const G4VTouchable *touchable = aStep->GetPreStepPoint()->GetTouchable();
    //questo volendo mi da l'indice del detector colpito dal fotone
    //G4int copyNo = touchable->GetCopyNumber();
    //G4cout << "copy number: "<< copyNo<<G4endl;
    G4VPhysicalVolume *physvol = touchable->GetVolume();
    G4ThreeVector posDetector = physvol->GetTranslation();
    G4cout << "detector position: "<< posDetector<<G4endl;

    return false;
}