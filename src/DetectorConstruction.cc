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
/// \file eventgenerator/particleGun/src/DetectorConstruction.cc
/// \brief Implementation of the DetectorConstruction class
//
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "DetectorConstruction.hh"

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"

DetectorConstruction::DetectorConstruction()
{  
  DefineMaterial();
}
DetectorConstruction::~DetectorConstruction(){}

void DetectorConstruction::DefineMaterial()
{
  G4NistManager *nist = G4NistManager::Instance();
  air = nist->FindOrBuildMaterial("G4_AIR");
  H2O = nist->FindOrBuildMaterial("G4_WATER");
  tungsteen = nist->FindOrBuildMaterial("G4_W");
}

G4VPhysicalVolume *DetectorConstruction::Construct()
{
  //https://geant4-userdoc.web.cern.ch/UsersGuides/ForApplicationDeveloper/html/Appendix/materialNames.html
  //dimensioni del mondo
  G4double xWorld = 0.5*m;
  G4double yWorld = 0.5*m;
  G4double zWorld = 0.5*m;
  
  solidWorld = new G4Box("solidWorld", xWorld, yWorld, zWorld);
  logicWorld = new G4LogicalVolume(solidWorld, air, "logicWorld");
  physWorld = new G4PVPlacement(0, G4ThreeVector(0.,0.,0.), logicWorld,"physWorld", 0, false, 0,true);

  solidRadiator = new G4Box("solidRadiator", 0.4*m, 0.4*m, 10.0*um);
  logicRadiator = new G4LogicalVolume(solidRadiator, tungsteen,"logicalRadiator");
  physRadiator = new G4PVPlacement(0,G4ThreeVector(0.,0.,0.1*m), logicRadiator,"physRadiatoar",logicWorld,false,0 );

  //rivelatori di fotoni
  solidDetector = new G4Box("solidDetector", xWorld, yWorld, 0.01*m);
  logicDetector = new G4LogicalVolume(solidDetector, air, "logicDetector");
  physDetector = new G4PVPlacement(0,G4ThreeVector(0.,0.,0.49*m), logicDetector,"physDetector",logicWorld,false,0 );

  return physWorld;
}

void DetectorConstruction::ConstructSDandField()
{
  SensitiveDetector *sensDet = new SensitiveDetector("SensitiveDetector");
  logicDetector->SetSensitiveDetector(sensDet);
}