#include <stepping.hh>
MySteppingAction::MySteppingAction(MyEventAction *eventAction){ fEventAction = eventAction;}
MySteppingAction::~MySteppingAction(){}
void MySteppingAction::UserSteppingAction(const G4Step* step)
{
    //se non ho capito male questo é passo della particella
    G4LogicalVolume *volume = step->GetPreStepPoint()->GetTouchableHandle()->GetVolume()->GetLogicalVolume();
    const MyDetectorConstruction *detectorConstruction = static_cast<const MyDetectorConstruction*>(G4RunManager::GetRunManager()->GetUserDetectorConstruction());
    G4LogicalVolume *fScoringVolume = detectorConstruction->GetScoringVolume();
    //voglio leggere l'energia depositata solo nel volume di interesse che ho settato prima
    //il cast lo faccio perché l'ho settato nei parametri di geant
    if(volume != fScoringVolume) return;
    G4double edep = step->GetTotalEnergyDeposit();//l'energia depositata nello step corrente
    fEventAction->AddEdep(edep);
}