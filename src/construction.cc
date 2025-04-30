#include "construction.hh"
#include <G4NistManager.hh>
#include <G4Box.hh>
#include <vector>

MyDetectorConstruction::MyDetectorConstruction()
{
    
    
    DefineMaterial();
}
MyDetectorConstruction::~MyDetectorConstruction(){}

// G4MaterialPropertyVector* ReadRindexCsv(std::string path)
// {
//     G4MaterialPropertyVector *matpropvec = nullptr;
//     std::ifstream datafile;
//     datafile.open(path);
//     std::vector<G4double> wlen_v, n_v;
//     while(1)
//     {
//         G4double wlen, n;
//         datafile>>wlen>>n;

//         if(datafile.eof()) break;
//         G4cout<<wlen<<";"<<n<<G4endl;

//         wlen_v.push_back(wlen);
//         n_v.push_back(n);
//     }
//     //G4OpticalMaterialProperties::ConvertToEnergy()
//     datafile.close();
//     G4OpticalMaterialProperties::ConvertToEnergy(wlen_v);
//     matpropvec = new G4MaterialPropertyVector(wlen_v, n_v);
//     return matpropvec;

// }

void MyDetectorConstruction::DefineMaterial()
{
    G4NistManager *nist = G4NistManager::Instance();
    //lista materiali nist
    //https://geant4-userdoc.web.cern.ch/UsersGuides/ForApplicationDeveloper/html/Appendix/materialNames.html
  
    G4MaterialPropertiesTable *mptAerogel = new G4MaterialPropertiesTable();
    //mptAerogel->AddProperty("RINDEX",energy, rindexAreogel, 2);
    //Aerogel->SetMaterialPropertiesTable(mptAerogel);
    

    tungsteen = nist->FindOrBuildMaterial("G4_Al");
    //leggo la tabella con l'indice di rifrazione del tungsteno scaricata da qua:
    //https://refractiveindex.info/?shelf=main&book=W&page=Ordal


    worldMat = nist->FindOrBuildMaterial("G4_AIR");
    //l'indice di rifrazione dell'aria é gia preimpostato, insieme a "PMMA" "Water" "Fused Silica"
    G4MaterialPropertiesTable *mptWorld = new G4MaterialPropertiesTable();
    mptWorld->AddProperty("RINDEX", "Air");
    worldMat->SetMaterialPropertiesTable(mptWorld);
    tungsteen->SetMaterialPropertiesTable(mptWorld);
}

G4VPhysicalVolume *MyDetectorConstruction::Construct()
{
    
    //dimensioni del mondo
    G4double xWorld = 0.5*m;
    G4double yWorld = 0.5*m;
    G4double zWorld = 0.5*m;

    solidWorld = new G4Box("World", xWorld, yWorld, zWorld);
    logicWorld = new G4LogicalVolume(solidWorld, worldMat, "World");
    physWorld = new G4PVPlacement(0, G4ThreeVector(0.,0.,0.), logicWorld,"World", 0, false, 0,true);

    solidRadiator = new G4Box("Radiator", 0.4*m, 0.4*m, 1*mm);
    logicRadiator = new G4LogicalVolume(solidRadiator, tungsteen,"Radiator");
    G4RotationMatrix* RotRadiator = new G4RotationMatrix;
    RotRadiator->rotateY(0*M_PI/180.0*rad);
    physRadiator = new G4PVPlacement(RotRadiator,G4ThreeVector(0.,0.,0.3*m), logicRadiator,"Radiator",logicWorld,false,0 );

    //uso il radiator come scoring volume, ovvero sommmo l'energia depositata su tutto il volume del radiator
    fScoringVolume = logicRadiator;


    solidDetector = new G4Box("Detector", xWorld, yWorld, 0.01*m);
    logicDetector = new G4LogicalVolume(solidDetector, worldMat, "Detector");
    G4RotationMatrix* RotDetector = new G4RotationMatrix;
    RotDetector->rotateY(M_PI/2.*0.0*rad);
    physDetector = new G4PVPlacement(RotDetector, G4ThreeVector(0*m,0*m,0.4*m), logicDetector,"Detector", logicWorld, false, 0,true);

    return physWorld;
}

void MyDetectorConstruction::ConstructSDandField()
{
    MySensitiveDetector *sensDet = new MySensitiveDetector("SensitiveDetector");
    logicDetector->SetSensitiveDetector(sensDet);

}