#ifndef CONSTRUCTION_HH
#define CONSTRUCTION_HH

#include <G4VUserDetectorConstruction.hh>
#include <G4SystemOfUnits.hh>
#include <G4LogicalVolume.hh>
#include <G4PVPlacement.hh>
//#include <G4OpticalMaterialProperties.hh>
#include <globals.hh>
#include <CLHEP/Vector/ThreeVector.h>
#include <G4GenericMessenger.hh>


#include <detector.hh>


class MyDetectorConstruction: public G4VUserDetectorConstruction
{
    public :
    MyDetectorConstruction();
    ~MyDetectorConstruction();

    G4LogicalVolume *GetScoringVolume() const {return fScoringVolume;}

    virtual G4VPhysicalVolume *Construct();

    //definisco il volume logico a cui dovro' accedere per creare il volume sensibile
    private:
    G4LogicalVolume *logicDetector;
    virtual void ConstructSDandField();
    G4GenericMessenger *fMessenger;
    G4int nCols,nRows;
    G4Box *solidWorld, *solidRadiator, *solidDetector;
    G4LogicalVolume *logicWorld, *logicRadiator, *fScoringVolume;
    G4VPhysicalVolume *physWorld, *physRadiator, *physDetector;
    G4Material *tungsteen, *worldMat;
    void DefineMaterial();
};


#endif