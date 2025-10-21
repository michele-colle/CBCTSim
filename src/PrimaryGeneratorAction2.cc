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
#include <TxtWithHeaderReader.hh>
#include <ranges>
#include <G4NistManager.hh>
#include <G4EmCalculator.hh>
#include <algorithm>
#include <EventInfo.hh>
#include <G4Geantino.hh>
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// PrimaryGeneratorAction2::PrimaryGeneratorAction2()
// {
//   G4int n_particle = 1;
//   fParticleGun = new G4ParticleGun(n_particle);
//   // G4ParticleDefinition* particle = G4ParticleTable::GetParticleTable()->FindParticle("proton");
//   fParticleGun->SetParticleDefinition(G4Gamma::Definition());
//    //fParticleGun->SetParticleDefinition(G4Geantino::GeantinoDefinition());

//   auto par = CBCTParams::Instance();

//   fParticleGun->SetParticlePosition(G4ThreeVector(0, -par->GetDSO(), 0));
//   G4cout << "distanza sorgente centro di rotazione: " << par->GetDSO() << G4endl;

//   // energy distribution
//   //
//   CreateSourceSpectrumWithFilters();
// }

PrimaryGeneratorAction2::PrimaryGeneratorAction2()
 : G4VUserPrimaryGeneratorAction(),
   fParticleSource(0)
{
  CreateSourceSpectrumWithFilters();
  auto par = CBCTParams::Instance();
//    G4int n_particle = 1;
//    fParticleGun = new G4ParticleGun(n_particle);
// fParticleGun->SetParticleEnergy(120*keV);
//    fParticleGun->SetParticlePosition(G4ThreeVector(0, -par->GetDSO(), 0));
//    fParticleGun->SetParticleDefinition(G4Gamma::Definition());
//    fParticleGun->SetParticleMomentumDirection(G4ThreeVector(0., 1., 0.));

  // 1. Create the G4GeneralParticleSource object
  fParticleSource = new G4GeneralParticleSource();

  // 2. Get a pointer to the source to configure it.
  //    GPS can manage multiple sources, but by default, there is one.
  G4SingleParticleSource* mySource = fParticleSource->GetCurrentSource();

  // 3. Set the particle type (e.g., a gamma for photons)
  //G4ParticleTable* particleTable = G4ParticleTable::GetParticleTable();
  //G4ParticleDefinition* particle = particleTable->FindParticle("gamma");
  mySource->SetParticleDefinition(G4Gamma::Definition());


  //===========================================================================
  // 4. DEFINE THE ENERGY SPECTRUM (User-defined Histogram)
  //===========================================================================
  G4SPSEneDistribution* energyDist = mySource->GetEneDist();
  energyDist->SetEnergyDisType("User"); // Set distribution type to User-defined

  // Define the histogram points (Energy, Relative Weight)
  // This creates a simple linear spectrum from 60 keV to 120 keV.
  // For a real simulation, you would load these from a file.


  for (G4int i = 0; i < fNPoints; ++i)
  {
    G4double energy = fX[i];
    G4double weight = fY[i];
    
    // Each call adds one (energy, weight) point to the internal histogram.
    energyDist->UserEnergyHisto(G4ThreeVector(energy, weight, 0.0));
  }
  //===========================================================================
  // 5. DEFINE THE ANGULAR DISTRIBUTION (Beam with a conical spread)
  //===========================================================================
  G4SPSAngDistribution* angDist = mySource->GetAngDist();
  angDist->SetAngDistType("iso"); // A 1D beam along a specified axis

  // Set the central axis of the beam (e.g., along the Z-axis)
  angDist->SetParticleMomentumDirection(G4ThreeVector(0., 1., 0.));//boh

  // Set the angular spread (sigma) of the beam.
  // This defines the half-angle of the cone.
  G4double alpha = std::atan(par->GetDetWidth()/2.0 /  par->GetDSD()); // half-angle in radians
  G4double beta = std::atan(par->GetDetHeight()/2.0 /  par->GetDSD()); // half-angle in radians
  angDist->SetMinTheta(90.0*deg - beta*rad); // Minimum theta angle
  angDist->SetMaxTheta(90.0*deg + beta*rad); // Maximum theta
  //non chiedetemi perché, ma funziona
  angDist->SetMinPhi( 270.0*deg-alpha*rad); // Minimum phi angle
  angDist->SetMaxPhi( 270.0*deg+alpha*rad ); // Maximum phi angle


  //===========================================================================
  // 6. DEFINE THE POSITION DISTRIBUTION (Optional but good practice)
  //===========================================================================
  G4SPSPosDistribution* posDist = mySource->GetPosDist();
  posDist->SetPosDisType("Point"); // A single point source
  posDist->SetCentreCoords(G4ThreeVector(0., -par->GetDSO(), 0.)); // Set its position


  G4int numberOfParticlesPerEvent = 100;
  mySource->SetNumberOfParticles(numberOfParticlesPerEvent);
}

PrimaryGeneratorAction2::~PrimaryGeneratorAction2()
{
}
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
void PrimaryGeneratorAction2::GeneratePrimaries(G4Event* anEvent)
{
  // This function is called once per event.

  // The GPS object already knows everything it needs to do:
  // - How many particles to generate (from SetNumberOfParticles or /gps/number)
  // - How to sample the energy for each one (from the EneDist settings)
  // - How to sample the direction for each one (from the AngDist settings)
  // - How to sample the position for each one (from the PosDist settings)
  
  // This single call tells GPS: "Do your thing for this event."
  // It will internally loop and generate all N particles according to its rules.
  fParticleSource->GeneratePrimaryVertex(anEvent);
  //fParticleGun ->GeneratePrimaryVertex(anEvent);
}
void PrimaryGeneratorAction2::GeneratePrimaries2(G4Event *anEvent)
{
  auto par = CBCTParams::Instance();
  // uniform solid angle
  G4double semiw = par->GetDetWidth() / 2.0;                   // semi-width
  G4double semih = par->GetDetHeight() / 2.0;                  // semi-height
  G4double fw = semiw * semiw / par->GetDSD() / par->GetDSD(); // width factor
  G4double fh = semih * semih / par->GetDSD() / par->GetDSD(); // height factor
  G4double xmax = sqrt(fw / (1. + fw));                        // x max
  G4double zmax = sqrt(fh / (1. + fh));                        // z max
  G4double xr = xmax * (1. - 2 * G4UniformRand());             //  uniform in [-1,+1]
  G4double zr = zmax * (1. - 2 * G4UniformRand());             //
  G4double yr = sqrt(1. - xr * xr - zr * zr);                  // y from x and z
  G4ThreeVector dir(xr, yr, zr);
  // G4cout<<dir<<G4endl;

  fParticleGun->SetParticleMomentumDirection(dir);
  //fParticleGun->SetParticleMomentumDirection(G4ThreeVector(0,1,0));

  // set energy from a tabulated distribution
  //
  // G4double energy = RejectAccept();
  G4double energy = InverseCumulSecondOrder();
  G4double eMin = 10 * keV;
  G4double eMax = 120 * keV;
  G4double Yrndm = eMin + G4UniformRand() * (eMax - eMin);
  // G4double energySelected = Yrndm;
  G4double energySelected = energy;
  // G4cout<<"energy: "<< energySelected<<G4endl;

  fParticleGun->SetParticleEnergy(energySelected);

  // // ATTENZIONE!!! questo non funziona perché scarterei il numero di particelle passato a particleGun!!!!! vedi sotto per dettagli
  // // salvo I0 pesando la probabilitá che questo venga contato dal rivelatore
  // if (scintillatorDetectorEfficiency && G4UniformRand() <= scintillatorDetectorEfficiency->Value(energySelected))
  // {
  //   // fParticleGun->SetParticleEnergy(10*MeV);
  //   G4AnalysisManager *analysis = G4AnalysisManager::Instance();
  //   analysis->FillH1(0, energySelected);
  //   // proietto sul detector
  //   G4double detx = par->GetDSD() * dir.x() / dir.y();
  //   G4double detz = par->GetDSD() * dir.z() / dir.y();
  //   analysis->FillH2(0, detx, detz, energySelected/keV);
  // }



  // G4cout<<"air detector position: "<<detx<<" "<< detz<<G4endl;

  // create vertex
  //
  particleCounter++;
  //G4cout<<"particleCounter: "<<particleCounter<<G4endl;
  //qui passa per il numero hiamato da run beam on, quindi con beamOn 10 passa 10 volte 
  //e spara il numero di particelle indicato in particleGun
  //dunque de ho particleGun(100) e beamOn 10 mi aspetto 10 direzioni ed energie diverse
  //e 100 particelle per ogni direzione
  fParticleGun->GeneratePrimaryVertex(anEvent);
  //G4PrimaryParticle* primary = anEvent->GetPrimaryVertex()->GetPrimary();
  
  //anEvent->SetUserInformation(new EventInfo(primary->GetMomentum(), primary->GetKineticEnergy(), particleCounter));

  // G4int nVtx = anEvent->GetNumberOfPrimaryVertex();
  // G4cout<<"NUMBER OF VERTEX "<<anEvent->GetNumberOfPrimaryVertex()<<G4endl;
  // G4cout<<"NUMBER OF Particle "<<anEvent->GetPrimaryVertex()->GetNumberOfParticle()<<G4endl;
  //  for (int i = 0; i < vertex->GetNumberOfParticle(); ++i)
  //   {
  //       G4PrimaryParticle* primary = vertex->GetPrimary(i);

  //       // For each primary particle, create a new TrackInfo object
  //       // containing ITS OWN momentum and attach it.
  //       primary->SetUserInformation(new TrackInfo(primary->GetMomentum()));
  //   }

}

void PrimaryGeneratorAction2::CreateSourceSpectrumWithFilters()
{
  auto par = CBCTParams::Instance();

  TxtWithHeaderReader readerSourceSpectrum;
  // Load the data file
  if (!readerSourceSpectrum.loadFromFile("IPEMXraysCatalogue.txt"))
  {
    std::cerr << "Failed to load data file" << std::endl;
    return;
  }

  fX = readerSourceSpectrum.getColumn("keV");
  for (auto &x : fX)
    x *= keV;

  fY = readerSourceSpectrum.getColumn(par->GetXRaySourceSpectrum());

  TxtWithHeaderReader filterSpectrum;
  G4cout << "Loading filter1 spectrum for material: " << par->GetFilter1Material() << G4endl;
  if (filterSpectrum.loadFromFile(par->GetFilter1Material() + ".txt"))
  {
    filterSpectrum.printSummary();
    filterSpectrum.printSample();
    auto enflt = filterSpectrum.getColumn("keV");
    auto att = filterSpectrum.getColumn("att");
    for (size_t i = 0; i < fY.size(); ++i)
    {
      if (enflt[i] * keV != fX[i])
      {
        G4cout << "Warning: energy mismatch in filter1 spectrum at index " << i << ": "
               << enflt[i] * keV << " != " << fX[i] << G4endl;
      }
      fY[i] *= std::exp(-par->GetFilter1Thickness() * att[i] / cm); // attenuazione
    }
  }
  if (filterSpectrum.loadFromFile(par->GetFilter2Material() + ".txt"))
  {
    auto enflt = filterSpectrum.getColumn("keV");
    auto att = filterSpectrum.getColumn("att");
    for (size_t i = 0; i < fY.size(); ++i)
    {
      if (enflt[i] * keV != fX[i])
      {
        G4cout << "Warning: energy mismatch in filter1 spectrum at index " << i << ": "
               << enflt[i] * keV << " != " << fX[i] << G4endl;
      }
      fY[i] *= std::exp(-par->GetFilter2Thickness() * att[i] / cm); // attenuazione
    }
  }

  for (auto &x : fY)
    G4cout << x << G4endl;

  // tabulated function
  // Y is assumed positive, linear per segment, continuous
  //
  fNPoints = fX.size();

  fYmax = *std::max_element(fY.begin(), fY.end());

  // compute slopes
  //
  fSlp.resize(fNPoints);
  for (G4int j = 0; j < fNPoints - 1; j++)
  {
    fSlp[j] = (fY[j + 1] - fY[j]) / (fX[j + 1] - fX[j]);
  };

  // compute cumulative function geantexamples, perché??
  //
  fYC2.resize(fNPoints);
  fYC2[0] = 0.;
  for (G4int j = 1; j < fNPoints; j++)
  {
    fYC2[j] = fYC2[j - 1] + 0.5 * (fY[j] + fY[j - 1]) * (fX[j] - fX[j - 1]);
  };
  // compute cumulative function gmik
  //
  fYC.resize(fNPoints);
  fYC[0] = 0.;
  for (G4int j = 1; j < fNPoints; j++)
  {
    fYC[j] = fYC[j - 1] + fY[j];
  };

  // codice duplicato ma vabe, c'e' di peggio nella vita
  TxtWithHeaderReader reader;
  std::cout << "Loading scintillator detector efficiency for material: " << par->GetDetectorMaterial() + ".txt" << std::endl;
  if (reader.loadFromFile(par->GetDetectorMaterial() + ".txt"))
  {
    scintillatorDetectorEfficiency = new G4PhysicsOrderedFreeVector();
    auto enflt = reader.getColumn("keV");
    auto att = reader.getColumn("att");
    for (size_t i = 0; i < enflt.size(); ++i)
    {
      scintillatorDetectorEfficiency->InsertValues(enflt[i] * keV, 1 - exp(-par->GetDetectorThickness() * att[i] / cm));
      std::cout << enflt[i] << " " << 1 - exp(-par->GetDetectorThickness() * att[i] / cm) << std::endl;
    }
  }
  else
  {
    std::cerr << "Failed to load scintillator detector efficiency data file" << std::endl;
    scintillatorDetectorEfficiency = nullptr;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4double PrimaryGeneratorAction2::RejectAccept()
{
  // tabulated function
  // Y is assumed positive, linear per segment, continuous
  // (see Particle Data Group: pdg.lbl.gov --> Monte Carlo techniques)
  //
  G4double Xrndm = 0., Yrndm = 0., Yinter = -1.;

  while (Yrndm > Yinter)
  {
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

G4double PrimaryGeneratorAction2::InverseCumulSecondOrder()
{
  // tabulated function
  // Y is assumed positive, linear per segment, continuous
  // --> cumulative function is second order polynomial
  // (see Particle Data Group: pdg.lbl.gov --> Monte Carlo techniques)

  // choose y randomly
  G4double Yrndm = G4UniformRand() * fYC2[fNPoints - 1];
  // find bin
  G4int j = fNPoints - 2;
  while ((fYC2[j] > Yrndm) && (j > 0))
    j--;
  // y_rndm --> x_rndm :  fYC(x) is second order polynomial
  G4double Xrndm = fX[j];
  G4double a = fSlp[j];
  if (a != 0.)
  {
    G4double b = fY[j] / a, c = 2 * (Yrndm - fYC2[j]) / a;
    G4double delta = b * b + c;
    G4int sign = 1;
    if (a < 0.)
      sign = -1;
    Xrndm += sign * std::sqrt(delta) - b;
  }
  else if (fY[j] > 0.)
  {
    Xrndm += (Yrndm - fYC2[j]) / fY[j];
  };
  return Xrndm;
}

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
  return Xrndm;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
