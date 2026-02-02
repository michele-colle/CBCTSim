#ifndef G4MATERIALTOHU_HH
#define G4MATERIALTOHU_HH 1

#include "globals.hh"
#include "G4SystemOfUnits.hh"
#include <itkImage.h>
#include "G4DatReader.hpp"

class G4MaterialToHU
{
public:
    G4MaterialToHU(G4double energy, G4DatReader::PhantomSex sex);
    ~G4MaterialToHU()= default;
    float GetHUForMaterial(int materialID) const {
        if(materialID < 0 || static_cast<size_t>(materialID) >= fMaterialToHUMap.size()){
            return -1000.0f; // Air
        }
        return fMaterialToHUMap[materialID];
    }

private:
    G4double fEnergy; // Energy at which the HU conversion is defined
    G4DatReader::PhantomSex fSex;
    std::vector<float> fMaterialToHUMap;
};
# endif