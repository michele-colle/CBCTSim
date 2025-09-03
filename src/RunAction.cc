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
/// \file eventgenerator/particleGun/src/RunAction.cc
/// \brief Implementation of the RunAction class
//
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "RunAction.hh"
#include "HistoManager.hh"
#include "G4Run.hh"
#include <filesystem>
#include <CBCTParams.hh>
#include <TxtWithHeaderReader.hh>
#include <PrimaryGeneratorAction2.hh>
#include "G4AnalysisManager.hh"
#include "G4UnitsTable.hh"

#ifdef WITH_CELERITAS
#include <CeleritasG4.hh>
#include <accel/TrackingManagerIntegration.hh>
#endif

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
std::atomic<long> RunAction::fEventsProcessed;

RunAction::RunAction()
{
  fHistoManager = new HistoManager();
  G4cout << "RunActionConstructor" << G4endl;
  fEventsProcessed.store(0);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::~RunAction()
{
  delete fHistoManager;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
void RunAction::BeginOfRunAction(const G4Run *run)
{
  // viene chiamato per ogni thread
  fStartTime = std::chrono::high_resolution_clock::now();
  fEventsProcessed.store(0);
#ifdef WITH_CELERITAS

  celeritas::TrackingManagerIntegration::Instance().BeginOfRunAction(run);
#endif
  // histograms
  //
  G4AnalysisManager *analysisManager = G4AnalysisManager::Instance();
  if (analysisManager->IsActive())
  {
    analysisManager->OpenFile();
    G4cout << "opening histogram" << G4endl;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::EndOfRunAction(const G4Run *run)
{
  TxtWithHeaderReader reader;

  // Load the data file
  if (!reader.loadFromFile("IPEMXraysCatalogue.txt"))
  {
    std::cerr << "Failed to load data file" << std::endl;
    return;
  }

  auto par = CBCTParams::Instance();

  auto fX = reader.getColumn("keV");
  for (auto &x : fX)
    x *= keV;
  auto fY = reader.getColumn(par->GetXRaySourceSpectrum());
  // Create histogram for spectrum
  // Create ntuple for spectrum
  // auto analysis = G4AnalysisManager::Instance();
  // auto id = analysis->CreateNtuple("spectrum_ntuple", "X-ray Spectrum Ntuple");
  // analysis->CreateNtupleDColumn(id, "energy_keV"); // or "energy" if you prefer
  // analysis->CreateNtupleDColumn(id, "intensity");
  // analysis->FinishNtuple(id);

  // // Fill ntuple with the spectrum data
  // for (size_t i = 0; i < fX.size(); ++i)
  // {
  //   analysis->FillNtupleDColumn(id, 0, fX[i]);
  //   analysis->FillNtupleDColumn(id, 1, fY[i]);
  //   analysis->AddNtupleRow(id);
  // }

  G4cout << "ed of run action" << G4endl;
  auto end_time = std::chrono::high_resolution_clock::now();
  auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - fStartTime).count();

  // Use ctime to format the seconds, as it's easier
  tm *ltm = localtime(&elapsed_time);
  std::cout << "elapsed time " << ltm->tm_hour << ":" << ltm->tm_min << ":" << ltm->tm_sec << ")" << std::endl;
  std::cout << "=> GetNumberOfEventsToBeProcessed() " << G4RunManager::GetRunManager()->GetNumberOfEventsToBeProcessed() << std::endl;
  std::cout << "=> total event counter " << fEventsProcessed.fetch_add(0, std::memory_order_relaxed) << std::endl;

  // save histograms
  //
  G4AnalysisManager *analysisManager = G4AnalysisManager::Instance();
  if (analysisManager->IsActive())
  {
    analysisManager->Write();
    auto file = analysisManager->GetFileName();
    analysisManager->CloseFile();
    G4cout << "Closing histogram... " << file << G4endl;

    // copio il file nella cartella root_macro
    std::filesystem::path outputPath(file.c_str());
    std::filesystem::path parentDir = std::filesystem::current_path().parent_path();
    std::filesystem::path targetDir = parentDir / "root_macro";
    std::filesystem::path targetPath = targetDir / outputPath.filename();
    try
    {
      std::filesystem::copy_file(outputPath, targetPath, std::filesystem::copy_options::overwrite_existing);
      G4cout << "copied histogram... " << targetPath << G4endl;
    }
    catch (const std::exception &e)
    {
      G4cerr << "Error copying histogram file: " << e.what() << G4endl;
      return;
    }
  }
#ifdef WITH_CELERITAS
  celeritas::TrackingManagerIntegration::Instance().EndOfRunAction(run);
#endif
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
