#include <detector.hh>
MySensitiveDetector::MySensitiveDetector(G4String name) : G4VSensitiveDetector(name)
{
    quEff = new G4PhysicsOrderedFreeVector(false);
    std::ifstream datafile;
    datafile.open("eff.dat");

    while(1)
    {
        G4double wlen, qeff;
        datafile>>wlen>>qeff;

        if(datafile.eof()) break;
        G4cout<<wlen<<" "<<qeff<<G4endl;

        quEff->InsertValues(wlen,qeff/100.);
    }
    datafile.close();
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
    //leggo il momento del fotone per calcolarmi la lungheza d'onada
    G4ThreeVector momPhoton = preStepPoint->GetMomentum();
    G4double wlen = (1.239841939*eV/momPhoton.mag())*1E+03;

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
    man->FillNtupleDColumn(0, 4, wlen);
    man->AddNtupleRow(0);

    //il trick é quello di calcolare l'efficienza con la lunghezza d'onda, poi generare un numero random tra 0 e 1 
    //e se il numero é minore della probabilitá calcolata allora il detector ha rilevato il fotone e lo includo
    if(G4UniformRand()<quEff->Value(wlen))
    {
        man->FillNtupleIColumn(1, 0, evt);
        man->FillNtupleDColumn(1, 1, posDetector[0]);
        man->FillNtupleDColumn(1, 2, posDetector[1]);
        man->FillNtupleDColumn(1, 3, posDetector[2]);
        man->AddNtupleRow(1);
    }





    return false;
}