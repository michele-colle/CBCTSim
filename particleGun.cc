//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
/// \file eventgenerator/particleGun/particleGun.cc
/// \brief Main program of the eventgenerator/particleGun example
//
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "ActionInitialization.hh"
#include "DetectorConstruction.hh"
#include "PhysicsList.hh"

#include "G4RunManagerFactory.hh"
#include "G4SteppingVerbose.hh"
#include "G4Types.hh"
#include "G4UIExecutive.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"
#include "Randomize.hh"
#include <Messenger.hh>
#include <TxtWithHeaderReader.hh>
#include "ICRP110PhantomConstruction.hh"

#ifdef WITH_CELERITAS

#include <CeleritasG4.hh>

using TMI = celeritas::TrackingManagerIntegration;
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....
celeritas::SetupOptions MakeOptions()
{
    celeritas::SetupOptions opts;
    // NOTE: these numbers are appropriate for CPU execution and can be set
    // through the UI using `/celer/`
    opts.max_num_tracks = 2024;
    opts.initializer_capacity = 2024 * 128;
    // Celeritas does not support EmStandard MSC physics above 200 MeV
    opts.ignore_processes = {"CoulombScat"};

    // Use a uniform (zero) magnetic field
    opts.make_along_step = celeritas::UniformAlongStepFactory();

    // Save diagnostic file to a unique name
    opts.output_file = "trackingmanager-offload.out.json";
    return opts;
}
#endif

int main(int argc, char** argv)
{
  // detect interactive mode (if no arguments) and define UI session
  G4UIExecutive* ui = 0;
  if (argc == 1) ui = new G4UIExecutive(argc, argv);

  // choose the Random engine
  CLHEP::HepRandom::setTheEngine(new CLHEP::RanecuEngine);

  // use G4SteppingVerboseWithUnits
  G4int precision = 4;
  G4SteppingVerbose::UseBestUnit(precision);

  // construct the run manager
  auto runManager = G4RunManagerFactory::CreateRunManager();

  // set mandatory initialization classesa
  DetectorConstruction* detector = new DetectorConstruction;
  //auto userPhantom = new ICRP110PhantomConstruction();

  runManager->SetUserInitialization(detector);


  // auto userPhantom = new ICRP110PhantomConstruction();
  // runManager -> SetUserInitialization(userPhantom);
#ifdef WITH_CELERITAS
  auto& tmi = TMI::Instance();
  auto* physics_list = new PhysicsList;
  physics_list->RegisterPhysics(new celeritas::TrackingManagerConstructor(&tmi));
  runManager->SetUserInitialization(physics_list);
  G4cout<<"Celeritassssss"<<G4endl;

#else
  runManager->SetUserInitialization(new PhysicsList);
  G4cout<<"Geant4 only"<<G4endl;
#endif

  //

  runManager->SetUserInitialization(new ActionInitialization);

  Messenger* mess = new Messenger();
   #ifdef WITH_CELERITAS

  tmi.SetOptions(MakeOptions());
#endif
  // initialize visualization
  G4VisManager* visManager = nullptr;

  // get the pointer to the User Interface manager
  G4UImanager* UImanager = G4UImanager::GetUIpointer();

  if (ui) {
    // interactive mode
    visManager = new G4VisExecutive;
    visManager->Initialize();
    G4String command = "/run/numberOfThreads 1";
    UImanager->ApplyCommand(command);
    UImanager->ApplyCommand("/control/execute vis.mac");
    ui->SessionStart();
    delete ui;
  }
  else {
    // batch mode
    // batch mode
    if (argc == 3) 
    {
      G4String command = "/run/numberOfThreads ";
      G4String nThreads = argv[2]; 
      UImanager->ApplyCommand(command + argv[2]);
      G4cout<<"numberOfThreads "<<argv[2]<<G4endl;

    }
    else
    {
      G4String command = "/run/useMaximumLogicalCores";
      UImanager->ApplyCommand(command);  
      G4cout<<"multithread maximum"<<G4endl;
    }
    G4String command = "/control/execute ";
    G4String fileName = argv[1];
    UImanager->ApplyCommand(command + fileName);
  }

  // job termination
  delete visManager;
  delete runManager;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....
