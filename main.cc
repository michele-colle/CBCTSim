#include <iostream>
#include "G4RunManager.hh"
#include "G4VisManager.hh"
#include "G4UIExecutive.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"

int main(int argc, char** argv) {
    G4UIExecutive* ui = nullptr;
    if (argc == 1) {
      ui = new G4UIExecutive(argc, argv);
    }
    // Initialize the run manager
    G4RunManager* runManager = new G4RunManager();
    G4VisManager* visManager = new G4VisExecutive();
    visManager->Initialize();
    G4UImanager* UImanager = G4UImanager::GetUIpointer();
    ui->SessionStart();

    return 0;
}