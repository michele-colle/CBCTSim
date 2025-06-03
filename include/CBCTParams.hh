// SimulationParameters.hh
#ifndef CBCT_PARAMS_HH
#define CBCT_PARAMS_HH

#include "globals.hh"
#include "G4ThreeVector.hh"
#include "G4SystemOfUnits.hh"
#include <G4Material.hh>

class CBCTParams {
public:
    static CBCTParams* Instance();  // Singleton getter

    void SetDSO(const G4double& dso) { 
        G4cout << "Setting DSO to: " << dso/mm << " mm" << G4endl;
        fDSO = dso; }
    G4double GetDSO() const { return fDSO; }

    void SetDSD(G4double val) { fDSD = val; }
    G4double GetDSD() const { return fDSD; }

    void SetDetWidth(G4double val) { fDetWidth = val; }
    G4double GetDetWidth() const { return fDetWidth; }

    void SetDetHeight(G4double val) { fDetHeight = val; }
    G4double GetDetHeight() const { return fDetHeight; }

    void SetFilter1Material(G4Material* mat) { fFilter1Mat = mat; }
    G4Material* GetFilter1Material() const { return fFilter1Mat; }

    void SetFilter2Material(G4Material* mat) { fFilter2Mat = mat; }
    G4Material* GetFilter2Material() const { return fFilter2Mat; }

    void SetFilter1Thickness(G4double val) { fFilter1Thickness = val; }
    G4double GetFilter1Thickness() const { return fFilter1Thickness; }

    void SetFilter2Thickness(G4double val) { fFilter2Thickness = val; }
    G4double GetFilter2Thickness() const { return fFilter2Thickness; }

    void SetDetectorMaterial(G4Material* mat) { fDetectorMaterial = mat; }
    G4Material* GetDetectorMaterial() const { return fDetectorMaterial; }

    void SetDetectorThickness(G4double val) { fDetectorThickness = val; }
    G4double GetDetectorThickness() const { return fDetectorThickness; }

    void SetObjectAngleInDegree(G4double val) { objectAngleInDegree = val; }
    G4double GetObjectAngleInDegree() const { return objectAngleInDegree; }


private:
    CBCTParams() = default;
    static CBCTParams* fInstance;

    G4double fDSO = 50.0 * mm;
    G4double fDSD = 100.0 * mm;
    G4double fDetWidth = 100.0 * mm;
    G4double fDetHeight = 100.0 * mm;
    G4Material* fFilter1Mat = nullptr;
    G4Material* fFilter2Mat = nullptr;
    G4double fFilter1Thickness = 0.0, fFilter2Thickness = 0.0;
    G4Material* fDetectorMaterial = nullptr;
    G4double fDetectorThickness = 0.0;
    G4double objectAngleInDegree = 0.0;

};

#endif
