#ifndef G4MATERIALTOHU_HH
#define G4MATERIALTOHU_HH 1

#include "globals.hh"
#include "G4SystemOfUnits.hh"
#include <itkImage.h>
#include "G4DatReader.hpp"
#include "G4RunManager.hh"

class G4MaterialToHU
{
public:
    G4MaterialToHU(G4double energy, G4DatReader::PhantomSex sex);
    ~G4MaterialToHU(){
        //delete runManager;
    };
    float GetHUForMaterial(int materialID) const {
        if(materialID < 0 || static_cast<size_t>(materialID) >= fMaterialToHUMap.size()){
            return -1000.0f; // Air
        }
        return fMaterialToHUMap[materialID];
    }
    static bool initialized;

private:
    G4double fEnergy; // Energy at which the HU conversion is defined
    G4DatReader::PhantomSex fSex;
    std::vector<float> fMaterialToHUMap;
    G4RunManager* runManager;
};
# endif