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
#include <CBCTParams.hh>

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include <G4Cons.hh>
#include <G4Tubs.hh>
#include <G4VisAttributes.hh>
#include <G4Colour.hh>

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
  auto par = CBCTParams::Instance();
  auto ddo = par->GetDSD()-par->GetDSO();

  auto maxz = std::max(par->GetDSO(), ddo);


  G4cout<<"DSD: "<<par->GetDSD()<<G4endl;
  G4cout<<"DSO: "<<par->GetDSO()<<G4endl;


  //https://geant4-userdoc.web.cern.ch/UsersGuides/ForApplicationDeveloper/html/Appendix/materialNames.html
  //dimensioni del mondo
  G4double xWorld = maxz+10*cm;
  G4double yWorld = maxz+10*cm;
  G4double zWorld = maxz+10*cm;
  
  solidWorld = new G4Box("World", xWorld, yWorld, zWorld);
  logicWorld = new G4LogicalVolume(solidWorld, air, "World");
  physWorld = new G4PVPlacement(0, G4ThreeVector(0.,0.,0.), logicWorld,"World", 0, false, 0,true);


  // --- Reconstruction Cylinder (mother for radiator) ---
  G4double reconRadius = ddo;
  G4double reconHeight = ddo;
  auto solidReconCyl = new G4Tubs("ReconCylinder", 0, reconRadius, reconHeight, 0, 360*deg);
  auto logicReconCyl = new G4LogicalVolume(solidReconCyl, air, "ReconCylinder");
  logicReconCyl->SetVisAttributes(new G4VisAttributes(G4Colour(1.0, 1.0, 0.0, 0.1)));//cilindro gialle

  // Rotation matrix for scan angle (around Y axis, for example)
  auto scanAngle = par->GetObjectAngleInDegree();
  auto reconRot = new G4RotationMatrix();
  reconRot->rotateX(90 * deg);
  reconRot->rotateY(scanAngle);

  //l'orientazione degli oggetti nel cilindro é come quella del dicom ma con la z al contrario, 
  //in pratica il piano x e y é il pavimento e la z sale verso l'alto, 
  //quindi é come se la particella venuisse sparata lungo l'asse -y
  solidRadiator = new G4Box("Radiator", 4*cm, 0.5*cm, 4*cm);
  logicRadiator = new G4LogicalVolume(solidRadiator, H2O,"Radiator");
  physRadiator = new G4PVPlacement(0,G4ThreeVector(0.,0.,0), logicRadiator,"Radiator",logicReconCyl,false,0 );


  // Place the reconstruction cylinder at the origin, rotated
  auto physReconCyl = new G4PVPlacement(
      reconRot, G4ThreeVector(0,0,0), logicReconCyl, "ReconCylinder",
      logicWorld, false, 0, true);


  //rivelatori di fotoni
  solidDetector = new G4Box("Detector", 0.5*par->GetDetWidth(), 0.5*par->GetDetHeight(), 0.5*cm);
  logicDetector = new G4LogicalVolume(solidDetector, air, "Detector");
  logicDetector->SetVisAttributes(new G4VisAttributes(G4Colour(0.0, 0.0, 1.0)));//detector blu
  //sposto il detecto di metá della sua profonditá per mantenere la ddo corretta
  physDetector = new G4PVPlacement(0,G4ThreeVector(0.,0.,ddo+0.5*cm), logicDetector,"Detector",logicWorld,false,0 );


  // creo un marker per la sorgente
  auto sourceMarkerSolid = new G4Cons("GunMarkerCone", 0, 1*cm, 0, 2*cm, 1*cm, 0, 360*deg);
  auto sourceMarkerLogical = new G4LogicalVolume(sourceMarkerSolid, air, "sourceMarkerLV");
  sourceMarkerLogical->SetVisAttributes(new G4VisAttributes(G4Colour(1.0, 0.0, 0.0)));
  physSourceMarker = new G4PVPlacement(0, G4ThreeVector(0, 0, -1.001*cm-par->GetDSO()), sourceMarkerLogical, "sourceMarker", logicWorld, false, 0, true);            


  return physWorld;
}

void DetectorConstruction::ConstructSDandField()
{
  SensitiveDetector *sensDet = new SensitiveDetector("SensitiveDetector");
  logicDetector->SetSensitiveDetector(sensDet);
}