#ifndef CONSTRUCTION_HH
#define CONSTRUCTION_HH

#include <G4VUserDetectorConstruction.hh>
#include <G4SystemOfUnits.hh>
#include <G4LogicalVolume.hh>
#include <G4PVPlacement.hh>
#include <globals.hh>
#include <CLHEP/Vector/ThreeVector.h>
#include <detector.hh>

class MyDetectorConstruction: public G4VUserDetectorConstruction
{
    public :
    MyDetectorConstruction();
    ~MyDetectorConstruction();

    virtual G4VPhysicalVolume *Construct();

    //definisco il volume logico a cui dovro' accedere per creare il volume sensibile
    private:
    G4LogicalVolume *logicDetector;
    virtual void ConstructSDandField();
};


#endif