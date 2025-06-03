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
/// \file eventgenerator/particleGun/src/PrimaryGeneratorAction2.cc
/// \brief Implementation of the PrimaryGeneratorAction2 class
//
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "PrimaryGeneratorAction2.hh"
#include <CBCTParams.hh>


#include "G4Event.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <G4Gamma.hh>
#include <G4AnalysisManager.hh>
#include <G4Proton.hh>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
PrimaryGeneratorAction2::PrimaryGeneratorAction2()
{
  fParticleGun = new G4ParticleGun(1);
  //G4ParticleDefinition* particle = G4ParticleTable::GetParticleTable()->FindParticle("proton");
  fParticleGun->SetParticleDefinition(G4Gamma::Definition());
  //fParticleGun->SetParticleDefinition(G4Proton::Definition());

  auto par = CBCTParams::Instance();


  fParticleGun->SetParticlePosition(G4ThreeVector(0., 0., -par->GetDSO()));
  G4cout<<"distanza sorgente centro di rotazione: "<<par->GetDSO()<<G4endl;

  // energy distribution
  //
  InitFunction();
  InitLogUniform(0.1*MeV, 10.0*MeV);
}
PrimaryGeneratorAction2::~PrimaryGeneratorAction2()
{}
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PrimaryGeneratorAction2::GeneratePrimaries(G4Event* anEvent)
{
  // uniform solid angle
  G4double cosAlpha = 1. - 2 * G4UniformRand();  // cosAlpha uniform in [-1,+1]
  G4double sinAlpha = std::sqrt(1. - cosAlpha * cosAlpha);
  G4double psi = twopi * G4UniformRand();  // psi uniform in [0, 2*pi]
  G4ThreeVector dir(sinAlpha * std::cos(psi), sinAlpha * std::sin(psi), cosAlpha);

  // fParticleGun->SetParticleMomentumDirection(dir);
  fParticleGun->SetParticleMomentumDirection(dir);

  // set energy from a tabulated distribution
  //
  // G4double energy = RejectAccept();
  G4double energy = InverseCumul();
  G4double eMin = 10*keV;
  G4double eMax = 120*keV;
  G4double Yrndm = eMin + G4UniformRand()*(eMax-eMin);
  G4double energylog = GetLogUniformRandom();
  G4double energySelected = Yrndm;
  fParticleGun->SetParticleEnergy(energySelected);
  //fParticleGun->SetParticleEnergy(10*MeV);

 //salvo I0
  G4AnalysisManager *analysis = G4AnalysisManager::Instance();
  analysis->FillH1(0, energySelected);


 // create vertex
  //
  fParticleGun->GeneratePrimaryVertex(anEvent);
}
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PrimaryGeneratorAction2::InitFunction()
{
  // tabulated function
  // Y is assumed positive, linear per segment, continuous
  //
  fNPoints = 16;
  const G4double xx[] = {37 * keV,  39 * keV,  45 * keV,  51 * keV, 57 * keV, 69 * keV,
                         71 * keV,  75 * keV,  83 * keV,  91 * keV, 97 * keV, 107 * keV,
                         125 * keV, 145 * keV, 159 * keV, 160 * keV};

  const G4double yy[] = {0.000,  0.077,  0.380,  2.044,  5.535, 15.077, 12.443, 14.766,
                         17.644, 18.518, 17.772, 14.776, 8.372, 3.217,  0.194,  0.000};

  // copy arrays in std::vector and compute fMax
  //
  fX.resize(fNPoints);
  fY.resize(fNPoints);
  fYmax = 0.;
  for (G4int j = 0; j < fNPoints; j++) {
    fX[j] = xx[j];
    fY[j] = yy[j];
    if (fYmax < fY[j]) fYmax = fY[j];
  };

  // compute slopes
  //
  fSlp.resize(fNPoints);
  for (G4int j = 0; j < fNPoints - 1; j++) {
    fSlp[j] = (fY[j + 1] - fY[j]) / (fX[j + 1] - fX[j]);
  };

  // compute cumulative function
  //
  fYC.resize(fNPoints);
  fYC[0] = 0.;
  for (G4int j = 1; j < fNPoints; j++) {
    fYC[j] = fYC[j - 1] + 0.5 * (fY[j] + fY[j - 1]) * (fX[j] - fX[j - 1]);
  };
}

void PrimaryGeneratorAction2::InitLogUniform(G4double start, G4double end)
{
  nPointsCumul = 10000;
  G4double step = (end-start)/(G4double)nPointsCumul;
  logUniformX.resize(nPointsCumul);
  logUniformCumul.resize(nPointsCumul);
  for (G4int j = 0; j < nPointsCumul; j++) 
  {
    G4double x = start+j*step;
    logUniformX[j] = x;
    logUniformCumul[j] = log(x/start)/log(end/start);
  }
}

G4double PrimaryGeneratorAction2::GetLogUniformRandom()
{
  G4double Yrndm = G4UniformRand()*logUniformCumul[nPointsCumul-1];
  G4int j = nPointsCumul - 2;
  while ((logUniformCumul[j] > Yrndm) && (j > 0))
    j--;

  // G4AnalysisManager* analysis = G4AnalysisManager::Instance();
  // analysis->FillH1(1, logUniformX[j]);
  return logUniformX[j];
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4double PrimaryGeneratorAction2::RejectAccept()
{
  // tabulated function
  // Y is assumed positive, linear per segment, continuous
  // (see Particle Data Group: pdg.lbl.gov --> Monte Carlo techniques)
  //
  G4double Xrndm = 0., Yrndm = 0., Yinter = -1.;

  while (Yrndm > Yinter) {
    // choose a point randomly
    Xrndm = fX[0] + G4UniformRand() * (fX[fNPoints - 1] - fX[0]);
    Yrndm = G4UniformRand() * fYmax;
    // find bin
    G4int j = fNPoints - 2;
    while ((fX[j] > Xrndm) && (j > 0))
      j--;
    // compute Y(x_rndm) by linear interpolation
    Yinter = fY[j] + fSlp[j] * (Xrndm - fX[j]);
  };
  return Xrndm;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4double PrimaryGeneratorAction2::InverseCumul()
{
  // tabulated function
  // Y is assumed positive, linear per segment, continuous
  // --> cumulative function is second order polynomial
  // (see Particle Data Group: pdg.lbl.gov --> Monte Carlo techniques)

  // choose y randomly
  G4double Yrndm = G4UniformRand() * fYC[fNPoints - 1];
  // find bin
  G4int j = fNPoints - 2;
  while ((fYC[j] > Yrndm) && (j > 0))
    j--;
  // y_rndm --> x_rndm :  fYC(x) is second order polynomial
  G4double Xrndm = fX[j];
  G4double a = fSlp[j];
  if (a != 0.) {
    G4double b = fY[j] / a, c = 2 * (Yrndm - fYC[j]) / a;
    G4double delta = b * b + c;
    G4int sign = 1;
    if (a < 0.) sign = -1;
    Xrndm += sign * std::sqrt(delta) - b;
  }
  else if (fY[j] > 0.) {
    Xrndm += (Yrndm - fYC[j]) / fY[j];
  };
  return Xrndm;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
