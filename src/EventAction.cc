
#include "EventAction.hh"
#include "RunAction.hh"
#include "G4Run.hh"
#include "G4Event.hh"


//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
EventAction::EventAction(RunAction* runAction) { }
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EventAction::BeginOfEventAction(const G4Event*anEvent)
{
//https://geant4-forum.web.cern.ch/t/how-to-output-time-since-start/6382/2
 // Run status
  nSecondaries = 0;
  G4int eventID=anEvent->GetEventID();
  G4Run* run = static_cast<G4Run*>( G4RunManager::GetRunManager()->GetNonConstCurrentRun() );
  G4int nOfEvents = run->GetNumberOfEventToBeProcessed();
  G4double dispPercentage = 1.; // status increment in dispPercentage

  if(fmod(eventID,double(nOfEvents*dispPercentage*0.01))==0)
  {
    time_t my_time = time(NULL);
    tm *ltm = localtime(&my_time);
    G4double status=(100*(eventID/double(nOfEvents))); 
    std::cout << "=> Event " << eventID << " starts ("<< status << "%, "<< ltm->tm_hour << ":" <<  ltm->tm_min << ":" << ltm->tm_sec << ")" << std::endl;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EventAction::EndOfEventAction(const G4Event*)
{
  //G4cout << "Number of secondaries: " << nSecondaries << G4endl;

  // accumulate statistics in run action
}

void EventAction::IncrementSecondaryParticleCounter()
{
  nSecondaries++;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
