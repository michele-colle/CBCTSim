#include "G4MaterialToHU.hh"
#include "ICRP110PhantomMaterial_Female.hh"
#include "ICRP110PhantomMaterial_Male.hh"
#include "G4EmCalculator.hh"
#include "G4NistManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4RunManager.hh"
#include "G4PhysListFactory.hh"
#include "G4EmCalculator.hh"
#include "G4NistManager.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4Gamma.hh"
#include "G4EmPenelopePhysics.hh"
#include "QGSP_BERT.hh"
#include "G4PhotoElectricEffect.hh"
#include "G4RunManager.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4PVPlacement.hh"
#include "G4LogicalVolume.hh"
#include "G4Box.hh"
#include "G4NistManager.hh"
#include "FTFP_BERT.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
std::vector<std::string> materialNames = {
  "air",
  "lung",
  "teeth",
  "bone",
  "humeri_upper",
  "humeri_lower",
  "arm_lower",
  "hand",
  "clavicle",
  "cranium",
  "femora_upper",
  "femora_lower",
  "leg_lower",
  "foot",
  "mandible",
  "pelvis",
  "ribs",
  "scapulae",
  "spine_cervical",
  "spine_lumbar",
  "spine_thoratic",
  "sacrum",
  "sternum",
  "hf_upper",
  "hf_lower",
  "med_lowerleg",
  "med_lowerarm",
  "cartilage",
  "skin",
  "blood",
  "muscle",
  "liver",
  "pancreas",
  "brain",
  "heart",
  "eye",
  "kidney",
  "stomach",
  "intestine_sml",
  "intestine_lrg",
  "spleen",
  "thyroid",
  "bladder",
  "ovaries_testes",
  "adrenals",
  "oesophagus",
  "misc",
  "uterus_prostate",
  "lymph",
  "breast_glandular",
  "breast_adipose",
  "gastro_content",
  "urine"};

// 1. MINIMAL GEOMETRY: Just a box of air
class MiniDetector : public G4VUserDetectorConstruction {
public:
    G4VPhysicalVolume* Construct() override {
        G4Material* air = G4NistManager::Instance()->FindOrBuildMaterial("G4_AIR");
        G4Box* box = new G4Box("World", 1*m, 1*m, 1*m);
        G4LogicalVolume* logicWorld = new G4LogicalVolume(box, air, "World");
        return new G4PVPlacement(0, G4ThreeVector(), logicWorld, "World", 0, false, 0);
    }
};

// 2. MINIMAL PARTICLE GUN: Shoots 1 GeV protons
class MiniGun : public G4VUserPrimaryGeneratorAction {
public:
    void GeneratePrimaries(G4Event* event) override {
        G4ParticleGun* gun = new G4ParticleGun(1);
        gun->SetParticleDefinition(G4Gamma::Definition()); // Simplified logic
        gun->GeneratePrimaryVertex(event);
    }
};

// 3. MINIMAL ACTION INITIALIZATION
class MiniAction : public G4VUserActionInitialization {
public:
    void Build() const override { SetUserAction(new MiniGun()); }
};

G4MaterialToHU::G4MaterialToHU(G4double energy, G4DatReader::PhantomSex sex) : fEnergy(energy), fSex(sex)
{
    //1. Setup the bare minimum Geant4 environment
    G4RunManager* runManager = new G4RunManager();
    runManager->SetUserInitialization(new MiniDetector());
    runManager->SetUserInitialization(new FTFP_BERT); // Pre-defined physics
    runManager->SetUserInitialization(new MiniAction());

    runManager->Initialize();
    runManager->BeamOn(10);
    
    G4EmCalculator emCalculator;
    G4NistManager* nist = G4NistManager::Instance();
    G4Material* water = nist->FindOrBuildMaterial("G4_WATER");

    G4double A = 14.01*g/mole;
    G4double Z;
    auto elN = new G4Element("Nitrogen","N", Z = 7.,A);
    A = 16.00*g/mole;
    auto elO = new G4Element("Oxygen","O",Z = 8.,A);
    G4double d = 0.001 *g/cm3;
    auto matAir = new G4Material("air",d,2);
    matAir -> AddElement(elN,0.8);
    matAir -> AddElement(elO,0.2);

    G4double muWater = 1.0 / (emCalculator.ComputeGammaAttenuationLength(energy, water)/cm);
    fMaterialToHUMap.resize(materialNames.size(),0.0f);
    if(sex == G4DatReader::PhantomSex::Male){
        // Initialize Male Material to HU Map
        auto pFemale = ICRP110PhantomMaterial_Female();
        pFemale.DefineMaterials();
        for(size_t i=0; i<materialNames.size(); ++i){
            G4Material* mat = pFemale.GetMaterial(materialNames[i]);
            if(mat){
                G4double muMat = 1.0 / (emCalculator.ComputeGammaAttenuationLength(energy, mat)/cm);
                float hu = static_cast<float>(1000.0 * (muMat - muWater) / muWater);
                fMaterialToHUMap[i] = hu;
            }
        }
    } 
    else {
        // Initialize Male Material to HU Map
        auto pMale = ICRP110PhantomMaterial_Male();
        pMale.DefineMaterials();
        for(size_t i=0; i<materialNames.size(); ++i){
            G4Material* mat = pMale.GetMaterial(materialNames[i]);
            if(mat){
                G4double muMat = 1.0 / (emCalculator.ComputeGammaAttenuationLength(energy, mat)/cm);
                float hu = static_cast<float>(1000.0 * (muMat - muWater) / muWater);
                fMaterialToHUMap[i] = hu;
            }
        }         
    }
    
}
