#include "construction.hh"
#include <G4NistManager.hh>
#include <G4Box.hh>

MyDetectorConstruction::MyDetectorConstruction(){}
MyDetectorConstruction::~MyDetectorConstruction(){}

G4VPhysicalVolume *MyDetectorConstruction::Construct()
{
    G4NistManager *nist = G4NistManager::Instance();

    //creo una box di silicio, carbonio e acqua per l'effetto cherenkov
    G4Material *SiO2 = new G4Material("SiO2", 2.201*g/cm3,2);
    SiO2->AddElement(nist->FindOrBuildElement("Si"),1);
    SiO2->AddElement(nist->FindOrBuildElement("O"),2);

    G4Material *H2O = new G4Material("H2O", 1.0*g/cm3,2);
    H2O->AddElement(nist->FindOrBuildElement("H"),2);
    H2O->AddElement(nist->FindOrBuildElement("O"),1);

    G4Element *C = nist->FindOrBuildElement("C");
    //effetto cherenkov, non mi ricordo cos'è quindi non ho capito bene cosa faccia
    G4Material *Aerogel = new G4Material("Aerogel", 0.2*g/cm3, 3);
    Aerogel->AddMaterial(SiO2, 62.5*perCent);
    Aerogel->AddMaterial(H2O, 37.4*perCent);
    Aerogel->AddElement(C, 0.1*perCent);
    //conversione energia lunghezza d'onda
    G4double energy[2] = {1.239841939*eV/0.9,1.239841939*eV/0.2};
    //lista di indici di rifrazione in base all'energia dei fotoni (di sopra)
    //per il momento sono uguali perchè approssimiamo uniforme l'indice di rifrazione 
    //0.2 é luce visibile blu (luncgezza d'onda in um?) 0.9 é rosso, circa...
    G4double rindexAreogel[2] = {1.1,1.1};
    G4MaterialPropertiesTable *mptAerogel = new G4MaterialPropertiesTable();
    mptAerogel->AddProperty("RINDEX",energy, rindexAreogel, 2);
    Aerogel->SetMaterialPropertiesTable(mptAerogel);

    

    

    //fine effetto cherenkov
    G4Material *worldMat = nist->FindOrBuildMaterial("G4_AIR");
    //indice di rifrazione per l'aria, altrimenti i fotoni non viaggiano nel mezzo!
    G4MaterialPropertiesTable *mptWorld = new G4MaterialPropertiesTable();
    G4double rindexAir[2] = {1.0,1.0};
    mptWorld->AddProperty("RINDEX", energy, rindexAir, 2);
    worldMat->SetMaterialPropertiesTable(mptWorld);
    G4Box *solidWorld = new G4Box("solidWorld", 0.5*m, 0.5*m, 0.5*m);

    G4LogicalVolume *logicWorld = new G4LogicalVolume(solidWorld, worldMat, "logicWorld");
    G4VPhysicalVolume *physWorld = new G4PVPlacement(0, G4ThreeVector(0.,0.,0.), logicWorld,"physWorld", 0, false, 0,true);
    G4Box *solidRadiator = new G4Box("solidRadiator", 0.4*m, 0.4*m, 0.01*m);
    G4LogicalVolume *logicRadiator = new G4LogicalVolume(solidRadiator, Aerogel,"logicalRadiator");
    G4VPhysicalVolume *physRadiator = new G4PVPlacement(0,G4ThreeVector(0.,0.,0.25*m), logicRadiator,"physRadiatoar",logicWorld,false,0 );
    return physWorld;
}