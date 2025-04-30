#include <iostream>
#include "G4RunManager.hh"
#include <G4MTRunManager.hh>
#include "G4VisManager.hh"
#include "G4UIExecutive.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"

#include <construction.hh>
#include <physics.hh>
#include <action.hh>

int main(int argc, char** argv) {

    G4UIExecutive* ui = nullptr;
    //se il numero dei parametri è uno vuol dire che non ho lanciato il sw con una macro
    //se non lancio il sw con una macro allora apro il visualizzatore, se invece lancio 
    //la macro spengo la visualizzazione cosi va piú veloce
    if (argc == 1) {
      ui = new G4UIExecutive(argc, argv);
    }
    // Initialize the run manager

    #ifdef G4MULTITHREADED
    G4MTRunManager *runManager = new G4MTRunManager();
    #else
    G4RunManager* runManager = new G4RunManager();
    #endif
    //G4RunManager* runManager = new G4RunManager();


    runManager->SetUserInitialization(new MyDetectorConstruction());
    runManager->SetUserInitialization(new MyPhysicsList());
    runManager->SetUserInitialization(new MyActionInitialization());
    //in multithreading va chiamato questa istruzione con il numero di thread, quindi la sposto nella macro
    //runManager->Initialize();

    G4VisManager* visManager = new G4VisExecutive();
    visManager->Initialize();

    G4UImanager* UImanager = G4UImanager::GetUIpointer();
    //se l'interfaccia esiste allora la apro
    if(ui)
    {
      //in pratica per lanciare l'interfaccia grafica devo chiamare vis/open
      //se non la chiamo non si apre
      UImanager->ApplyCommand("/control/execute run.mac");
      UImanager->ApplyCommand("/control/execute vis.mac");
      
      ui->SessionStart();
    }
    else
    {
      G4String command = "/control/execute ";
      G4String filename = argv[1];
      UImanager->ApplyCommand(command+filename);
    }

    return 0;
}