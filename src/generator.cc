#include <generator.hh>

MyPrimaryGenerator::MyPrimaryGenerator()
{
    fParticleGun = new G4ParticleGun(1);
    //creo qua la particella cosi da poter fare l' override direttamente dalle macro
    G4ParticleTable *particleTable = G4ParticleTable::GetParticleTable();
    // G4String particleName ="electron";
    G4ParticleDefinition *particle = particleTable->FindParticle("gamma");

    G4ThreeVector pos(0.,0.,0.);
    G4ThreeVector mom(0.,0.,1.);

    fParticleGun->SetParticleDefinition(particle);
    //fParticleGun->SetParticleDefinition(G4Electron::Definition());
    fParticleGun->SetParticlePosition(pos);
    fParticleGun->SetParticleMomentumDirection(mom);
    fParticleGun->SetParticleEnergy(120*keV);

}
MyPrimaryGenerator::~MyPrimaryGenerator()
{
    delete fParticleGun;
}
void MyPrimaryGenerator::GeneratePrimaries(G4Event *anEvent)
{
    //quello che faccio qua non viene influenzato dalle macro
    fParticleGun->GeneratePrimaryVertex(anEvent);
}
