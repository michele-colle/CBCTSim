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
/// \file eventgenerator/particleGun/src/PhysicsList.cc
/// \brief Implementation of the PhysicsList class
//
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "PhysicsList.hh"
#include <G4EmPenelopePhysics.hh>
#include <G4PhysListFactory.hh>
#include <G4EmLivermorePhysics.hh>
#include <G4Gamma.hh>
#include <G4PhotoElectricEffect.hh>
#include <G4ComptonScattering.hh>
#include <G4GammaConversion.hh>
#include <G4RayleighScattering.hh>
#include <G4ProcessManager.hh>

PhysicsList::PhysicsList()
{
    //G4PhysListFactory factory;
    //G4VModularPhysicsList* physList=factory.GetReferencePhysList("LIV");
    //physList->RegisterPhysics(new G4OpticalPhysics);
    // physList->RegisterPhysics(new G4EmStandardPhysics);
    // physList->ReplacePhysics(new G4EmLivermorePhysics);

    //RegisterPhysics(new G4EmStandardPhysics());
    RegisterPhysics(new G4EmPenelopePhysics());
    // RegisterPhysics(new G4EmLivermorePhysics());
    // RegisterPhysics(new G4OpticalPhysics()); 
    
}

PhysicsList::~PhysicsList(){}

// void PhysicsList::ConstructParticle()
// {
//       G4Gamma::GammaDefinition();

// }

// void PhysicsList::ConstructProcess()
// {
//     physList->RegisterPhysics(new G4ComptonScattering);
//     G4ProcessManager* pmanager = G4Gamma::Definition()->GetProcessManager();
//     pmanager->AddDiscreteProcess(new G4PhotoElectricEffect);
//     pmanager->AddDiscreteProcess(new G4ComptonScattering);
//     pmanager->AddDiscreteProcess(new G4GammaConversion);
//     pmanager->AddDiscreteProcess(new G4RayleighScattering); 
// }


