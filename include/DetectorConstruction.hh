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
/// \file eventgenerator/particleGun/include/DetectorConstruction.hh
/// \brief Definition of the DetectorConstruction class
//
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#ifndef DetectorConstruction_h
#define DetectorConstruction_h 1

#include <G4VUserDetectorConstruction.hh>
#include <G4SystemOfUnits.hh>
#include <G4LogicalVolume.hh>
#include <G4PVPlacement.hh>
#include <globals.hh>
#include <CLHEP/Vector/ThreeVector.h>
#include <G4GenericMessenger.hh>


#include <detector.hh>


class DetectorConstruction: public G4VUserDetectorConstruction
{
    public :
    DetectorConstruction();
    ~DetectorConstruction();

    virtual G4VPhysicalVolume *Construct();

    //definisco il volume logico a cui dovro' accedere per creare il volume sensibile
    private:
    G4LogicalVolume *logicDetector;
    virtual void ConstructSDandField();
    //G4GenericMessenger *fMessenger;
    G4Box *solidWorld, *solidDetector;
    G4LogicalVolume *logicWorld;
    G4VPhysicalVolume *physWorld, *physRadiator, *physDetector, *physSourceMarker, *physReconstructionCylinder;
    G4Material *H2O, *air, *tungsteen;
    void DefineMaterial();
};
#endif
