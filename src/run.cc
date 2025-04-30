#include <run.hh>
MyRunAction::MyRunAction()
{
    //singleton, basta chiamare instance
    G4AnalysisManager *man = G4AnalysisManager::Instance();

    //questa ntupla é per i dati "monte carlo" ovvero parametri non osservabili che utilizzeremo per questioni tipo efficienza quantica ecc...
    man->CreateNtuple("Photons","Photons");
    man->CreateNtupleIColumn("fEvent");
    man->CreateNtupleDColumn("fX");
    man->CreateNtupleDColumn("fY");
    man->CreateNtupleDColumn("fZ");
    man->CreateNtupleDColumn("fWlen");
    man->FinishNtuple(0);
  


}
MyRunAction::~MyRunAction(){}

void MyRunAction::BeginOfRunAction(const G4Run * run)
{
    
    //singleton, basta chiamare instance
    G4AnalysisManager *man = G4AnalysisManager::Instance();
    G4int runNumber = run->GetRunID();
    std::stringstream strRunId;
    strRunId<<runNumber;
    man->OpenFile("output"+strRunId.str()+".root");
 
}
void MyRunAction::EndOfRunAction(const G4Run * run)
{
    //singleton, basta chiamare instance
    G4AnalysisManager *man = G4AnalysisManager::Instance();
    man->Write();
    man->CloseFile();

}
