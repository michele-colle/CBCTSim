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
    auto mass = preStepPoint->GetMass();
    if(mass!=0)
        return false;

    G4ThreeVector posPhoton = preStepPoint->GetPosition();
    //leggo il momento del fotone per calcolarmi la lungheza d'onada
    G4double energy = preStepPoint->GetKineticEnergy();
    G4cout << " Photon energy: "<< energy<<G4endl;

    //usando questa funzione si possono stampare le posizioni di ingresso dei fotoni nel detector
    //G4cout << " Photon position: "<< posPhoton<<G4endl;
    const G4VTouchable *touchable = aStep->GetPreStepPoint()->GetTouchable();
    //questo volendo mi da l'indice del detector colpito dal fotone
    //G4int copyNo = touchable->GetCopyNumber();
    //G4cout << "copy number: "<< copyNo<<G4endl;
    G4VPhysicalVolume *physvol = touchable->GetVolume();
    G4ThreeVector posDetector = physvol->GetTranslation();
    //G4cout << "detector position: "<< posDetector<<G4endl;

    //per il futuro
    //https://indico.cern.ch/event/294651/sessions/55918/attachments/552022/760640/UserActions.pdf
    // G4double edep = aStep->GetTotalEnergyDeposit();
    // if(edep>0)
    //     G4cout << "detector position: "<< posDetector<<" energy: " <<edep<<G4endl;
    G4int evt = G4RunManager::GetRunManager()->GetCurrentEvent()->GetEventID();
    G4AnalysisManager *man = G4AnalysisManager::Instance();

    //specifico il numero dell ntupla
    man->FillNtupleIColumn(0, 0, evt);
    man->FillNtupleDColumn(0, 1, posPhoton[0]);
    man->FillNtupleDColumn(0, 2, posPhoton[1]);
    man->FillNtupleDColumn(0, 3, posPhoton[2]);
    man->FillNtupleDColumn(0, 4, energy/keV);
    man->AddNtupleRow(0);


    return false;
}