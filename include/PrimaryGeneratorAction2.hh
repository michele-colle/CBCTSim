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
/// \file eventgenerator/particleGun/include/PrimaryGeneratorAction2.hh
/// \brief Definition of the PrimaryGeneratorAction2 class
//
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#ifndef PrimaryGeneratorAction2_h
#define PrimaryGeneratorAction2_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "globals.hh"
#include <G4PhysicsOrderedFreeVector.hh>


#include <vector>

class G4ParticleGun;
class G4Event;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

class PrimaryGeneratorAction2: public G4VUserPrimaryGeneratorAction
{
  public:
    PrimaryGeneratorAction2();
    ~PrimaryGeneratorAction2() override;

  public:
    void GeneratePrimaries(G4Event*) override;

  public:
    G4ParticleGun*    GetParticleGun() { return fParticleGun; };

  public:
    G4double RejectAccept();
    G4double InverseCumulSecondOrder();
    G4double InverseCumul();
    //questa va chiamata da runAction perché per calcolare lo spettro
    //dei filtri devo aver attivato il kernel di Geant4, qualsiasi cosa vioglia dire.
    // in pratica se calcolo la cross section in questo costruttore mi ritorna tutti zeri
    void CreateSourceSpectrumWithFilters();


  private:
    G4ParticleGun* fParticleGun = nullptr;

    G4int fNPoints = 0;  // nb of points
    std::vector<G4double> fX;  // abscisses X
    std::vector<G4double> fY;  // values of Y(X)
    std::vector<G4double> fSlp;  // slopes
    std::vector<G4double> fYC2;  // cumulative function of Y
    std::vector<G4double> fYC;  // cumulative function of Y
    G4double fYmax = 0.;  // max(Y)
    G4PhysicsOrderedFreeVector* scintillatorDetectorEfficiency;
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif
