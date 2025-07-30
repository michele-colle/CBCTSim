// SimulationParameters.hh
#ifndef CBCT_PARAMS_HH
#define CBCT_PARAMS_HH

#include "globals.hh"
#include "G4ThreeVector.hh"
#include "G4SystemOfUnits.hh"
#include <G4Material.hh>

#include <G4NistManager.hh>
#include <G4EmCalculator.hh>
#include <G4Gamma.hh>
#include <G4ParticleDefinition.hh>
#include <G4ParticleTable.hh>
#include <G4UnitsTable.hh>
#include <G4String.hh>     

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

    void SetFilter1Material(const G4String& mat) 
    { 
        fFilter1Mat = mat; 
    }
    G4String GetFilter1Material() const { return fFilter1Mat; }

    void SetFilter2Material(const G4String& mat) { fFilter2Mat = mat; }
    G4String GetFilter2Material() const { return fFilter2Mat; }

    void SetFilter1Thickness(G4double val) { fFilter1Thickness = val; }
    G4double GetFilter1Thickness() const { return fFilter1Thickness; }

    void SetFilter2Thickness(G4double val) { fFilter2Thickness = val; }
    G4double GetFilter2Thickness() const { return fFilter2Thickness; }

    void SetDetectorMaterial(const G4String& mat) { fDetectorMaterial = mat; }
    G4String GetDetectorMaterial() const { return fDetectorMaterial; }

    void SetDetectorThickness(G4double val) { fDetectorThickness = val; }
    G4double GetDetectorThickness() const { return fDetectorThickness; }

    void SetObjectAngleInDegree(G4double val) { objectAngleInDegree = val; }
    G4double GetObjectAngleInDegree() const { return objectAngleInDegree; }

    void SetXRaySourceSpectrum(const G4String& spectrum) { XRaySourceSpectrum = spectrum; }
    G4String GetXRaySourceSpectrum() const { return XRaySourceSpectrum; }

    /// @brief Create a material spectrum based on the given material name
    /// @param material The name of the material for which to create the spectrum
    /// This function computes the attenuation spectrum for the specified material
    /// and saves the results to a file in the build folder, this must be called after trun initialize and
    /// before running a simulation with this materials.
    void CreateMaterialSpectrum(const G4String& material);


    void SetPatienStandPosition(G4double val) { fPatientStandPosition = val; }
    G4double GetPatienStandPosition() const { return fPatientStandPosition; }



private:
    CBCTParams() = default;
    static CBCTParams* fInstance;

    G4double fDSO = 50.0 * mm;
    G4double fDSD = 100.0 * mm;
    G4double fDetWidth = 100.0 * mm;
    G4double fDetHeight = 100.0 * mm;
    G4String fFilter1Mat = "";
    G4String fFilter2Mat = "";
    G4double fFilter1Thickness = 0.0, fFilter2Thickness = 0.0;
    G4String fDetectorMaterial = "";
    G4double fDetectorThickness = 0.0;
    G4double objectAngleInDegree = 0.0;
    G4String XRaySourceSpectrum = "120K10DW"; // Added member
    G4double fPatientStandPosition = 0.0;



};

#endif
