#include <detector.hh>
#include <G4Gamma.hh>
#include <TrackInfo.hh>
SensitiveDetector::SensitiveDetector(G4String name) : G4VSensitiveDetector(name)
{
}
SensitiveDetector::~SensitiveDetector() {}

G4bool SensitiveDetector::ProcessHits(G4Step *aStep, G4TouchableHistory *ROhist)
{
  // qua sembrano parlare di come discriminare lo scatter
  // https://www.researchgate.net/post/How-to-stop-particles-tracking-in-GEANT4
  G4Track *track = aStep->GetTrack();
  // questo impedisce la generazione di fotoni nel materiale del detector (glare?)
  track->SetTrackStatus(fStopAndKill);
  if (track->GetDefinition() != G4Gamma::Definition())
    return false;

  G4StepPoint *preStepPoint = aStep->GetPreStepPoint();
  G4StepPoint *postStepPoint = aStep->GetPostStepPoint();

  G4ThreeVector posPhoton = preStepPoint->GetPosition();
  // leggo il momento del fotone per calcolarmi la lungheza d'onada
  G4ThreeVector momPhoton = preStepPoint->GetMomentum();
  G4double wlen = (1.239841939 * eV / momPhoton.mag()) * 1E+03;
  G4double en = preStepPoint->GetKineticEnergy();

  //auto *info = dynamic_cast<TrackInfo *>(track->GetUserInformation());
  G4AnalysisManager *analysis = G4AnalysisManager::Instance();
  analysis->FillH1(1, en);

  if (false)
  {
    // G4ThreeVector p = info->primaryMomentum;
    // G4double E = info->primaryEnergy;
    // G4double diffSquared = (momPhoton/momPhoton.mag() - p/p.mag()).mag();
    // const bool isCollinear = (diffSquared == 0);
    // G4double deltaEnergy = E-en;
    // // const bool isCollinear = (diffSquared < DBL_EPSILON);
    // // G4cout<<"mom angle diff "<<diffSquared<<G4endl;
    // // G4cout<<"parent id "<<track->GetParentID()<<G4endl;
    // // G4cout<<"primary id "<<info->primaryID<<G4endl;
    // // G4cout<<"actual id "<<track->GetTrackID() <<G4endl;
    // // G4cout<<"primary momdir "<<p/p.mag()<<G4endl;
    // // G4cout<<"actual momdir "<<momPhoton/momPhoton.mag()<<G4endl;

    // // primary
    // if (deltaEnergy == 0 && isCollinear)
    // {
    //   analysis->FillH1(1, en);
    // }
    // else//scatter collinear and not collinear
    // {
    //   analysis->FillH1(isCollinear ? 2 : 3, en);
      
    // }


      
  }
  else{
          //G4cout<<"asfiojafiojaopdfijapodfijapdosfijapodifjpioj   "<<en<<G4endl;

  }

  // G4cout<<"energy ararara"<<en<<G4endl;

// const G4VProcess *process = aStep->GetPostStepPoint()->GetProcessDefinedStep();
//       G4String processName = " UserLimit";
//       if (process)
//       {
//         processName = process->GetProcessName();
//         if(processName!="Transportation"){
//           G4cout << "*Gamma - Process:  " << processName  <<G4endl;
//           const bool isCollinear = (diffSquared < DBL_EPSILON);
//           G4cout<<"mom angle diff "<<diffSquared<<G4endl;
//           G4cout<<"parent id "<<track->GetParentID()<<G4endl;
//           G4cout<<"primary id "<<info->primaryID<<G4endl;
//           G4cout<<"actual id "<<track->GetTrackID() <<G4endl;
//           G4cout<<"primary momdir "<<p/p.mag()<<G4endl;
//           G4cout<<"actual momdir "<<momPhoton/momPhoton.mag()<<G4endl<<G4endl<<G4endl;
//         }
//       }

  return false;
}