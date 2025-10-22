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
#include "G4Polyline.hh"
#include "G4Colour.hh"
#include "G4VisAttributes.hh"
#include "G4VVisManager.hh"
#include "G4Point3D.hh"

#include "ICRP110PhantomConstruction.hh"


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
  auto dsd = par->GetDSD();
  auto dso = par->GetDSO();
  auto ddo = dsd-dso;
  auto detwidth = par->GetDetWidth();

  auto maxxy = std::max(par->GetDSO(), ddo);
  auto maxz = par->GetDetHeight()/2.0;


  G4cout<<"DSD: "<<par->GetDSD()<<G4endl;
  G4cout<<"DSO: "<<par->GetDSO()<<G4endl;


  G4NistManager *nist = G4NistManager::Instance();
  auto bone =  nist->FindOrBuildMaterial("G4_BONE_COMPACT_ICRU");
  auto al =  nist->FindOrBuildMaterial("G4_Al");
  //https://geant4-userdoc.web.cern.ch/UsersGuides/ForApplicationDeveloper/html/Appendix/materialNames.html
  //dimensioni del mondo
  G4double xWorld = maxxy+10*cm;
  G4double yWorld = maxxy+10*cm;
  G4double zWorld = maxz+10*cm;
  
  solidWorld = new G4Box("World", xWorld, yWorld, zWorld);
  logicWorld = new G4LogicalVolume(solidWorld, air, "World");
  physWorld = new G4PVPlacement(0, G4ThreeVector(0.,0.,0.), logicWorld,"World", 0, false, 0,true);


  // --- Reconstruction Cylinder (mother for radiator) ---
  G4double reconRadius = dso/dsd* detwidth/2.0; // Radius of the cylinder based on DSO and DSD
  G4double reconHeight = ddo;
  auto solidReconCyl = new G4Tubs("ReconCylinder", 0, std::min(ddo, dso), maxz, 0, 360*deg);
  auto logicReconCyl = new G4LogicalVolume(solidReconCyl, air, "ReconCylinder");
  logicReconCyl->SetVisAttributes(new G4VisAttributes(G4Colour(1.0, 0.0, 1.0, 0.2)));//cilindro viola

  // Rotation matrix for scan angle (around Y axis, for example)
  auto scanAngle = par->GetObjectAngleInDegree();
  auto rotRecon = new G4RotationMatrix();
  rotRecon->rotateZ(scanAngle);

  auto solidRadiator = new G4Tubs("Radiator",2*cm, 3*cm, 8*cm,0, 360*deg);
  //auto solidRadiator = new G4Box("Radiator",5*cm, 5*cm/2.0,5*cm);
  auto logicRadiator = new G4LogicalVolume(solidRadiator, al,"Radiator");
  //physRadiator = new G4PVPlacement(0,G4ThreeVector(4*cm,0.,0), logicRadiator,"Radiator",logicReconCyl,false,0 );

  auto solidRadiator2 = new G4Tubs("Radiator2",0, 5*cm, 8*cm,0, 360*deg);;
  auto logicRadiator2 = new G4LogicalVolume(solidRadiator2, H2O,"Radiator2");
  //new G4PVPlacement(0,G4ThreeVector(0*cm,0,0), logicRadiator2,"Radiator2",logicReconCyl,false,0 );

  auto solidRadiator3 = new G4Tubs("Radiator",3.01*cm, 5*cm, 4*cm,0, 360*deg);
  auto logicRadiator3 = new G4LogicalVolume(solidRadiator3, H2O,"Radiator2");
  if(par->GetPhantom()=="WaterCylinder") {
    new G4PVPlacement(0,G4ThreeVector(0,0,0), logicRadiator3,"Radiator2",logicReconCyl,false,0 );
  }
  //


  // Place the reconstruction cylinder at the origin, rotated
  auto physReconCyl = new G4PVPlacement(
      rotRecon, G4ThreeVector(0,0,0), logicReconCyl, "ReconCylinder",
      logicWorld, false, 0, true);




  //creo il phantmo antropomorfo
  if(par->GetPhantom()=="HeadFemale") {
    auto userPhantom = new ICRP110PhantomConstruction();
    userPhantom->PlacePhantomInVolume(logicReconCyl);
  }
  

  //aggiungo la barella
  auto patientStandPosition = par->GetPatienStandPosition();
  auto patientStand = new G4Box("PatientStand", 25*cm, 1.5*cm, maxz);
  auto logicPatientStand = new G4LogicalVolume(patientStand, H2O, "PatientStand");
  logicPatientStand->SetVisAttributes(new G4VisAttributes(G4Colour(0.5, 0.5, 0.5))); // Grigio
  //new G4PVPlacement(0,G4ThreeVector(0,patientStandPosition,0), logicPatientStand,"PatientStand",logicReconCyl,false,0 );





  //rivelatori di fotoni
  solidDetector = new G4Box("solidDetector", 0.5*par->GetDetWidth(), 0.5*cm, 0.5*par->GetDetHeight());
  logicDetector = new G4LogicalVolume(solidDetector, air, "logicDetector");
  logicDetector->SetVisAttributes(new G4VisAttributes(G4Colour(0.0, 0.0, 1.0)));//detector blu
  //sposto il detecto di metá della sua profonditá per mantenere la ddo corretta
  physDetector = new G4PVPlacement(0,G4ThreeVector(0.,ddo+0.5*cm,0.), logicDetector,"physDetector",logicWorld,false,0 );

  // creo un marker per la sorgente
  auto sourceMarkerSolid = new G4Cons("GunMarkerCone", 0, 1*cm, 0, 2*cm, 1*cm, 0, 360*deg);
  auto sourceMarkerLogical = new G4LogicalVolume(sourceMarkerSolid, air, "sourceMarkerLV");
  sourceMarkerLogical->SetVisAttributes(new G4VisAttributes(G4Colour(1.0, 0.0, 0.0)));
  auto rotMarker = new G4RotationMatrix();
  rotMarker->rotateX(90*deg); // Rotate the marker to point upwards
  physSourceMarker = new G4PVPlacement(rotMarker, G4ThreeVector(0, -1.001*cm-par->GetDSO(), 0), sourceMarkerLogical, "sourceMarker", logicWorld, false, 0, true);            


  

  

  return physWorld;
}

void DetectorConstruction::ConstructSDandField()
{
  SensitiveDetector *sensDet = new SensitiveDetector("SensitiveDetector");
  G4SDManager::GetSDMpointer()->AddNewDetector(sensDet);

  logicDetector->SetSensitiveDetector(sensDet);

  G4cout << "Sensitive Detector set for Detector volume."<<logicDetector<< G4endl;
}