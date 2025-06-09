// CBCTParams.cc
#include "CBCTParams.hh"
#include <TxtWithHeaderReader.hh>

CBCTParams* CBCTParams::fInstance = nullptr;

CBCTParams* CBCTParams::Instance() {
    if (!fInstance) {
        fInstance = new CBCTParams();
    }
    return fInstance;
}
void CBCTParams::CreateMaterialSpectrum(const G4String& matName) 
{
    //leggo il file delle sorgenti per avere l'energia a cui servono le attenuazioni
    TxtWithHeaderReader readerSourceSpectrum;

    // Load the data file
    if (!readerSourceSpectrum.loadFromFile("IPEMXraysCatalogue.txt"))
    {
        std::cerr << "Failed to load data file" << std::endl;
        return;
    }



    auto sourceEnergyValues = readerSourceSpectrum.getColumn("keV");
    
    std::vector<G4double> attenuationSpectrum;
    G4NistManager *nist = G4NistManager::Instance();
    G4Material *material = nist->FindOrBuildMaterial(matName); // Aluminum
    
    G4EmCalculator calc;  
    
    
    material->GetMassOfMolecule();
    
    const G4ParticleDefinition* gamma = G4Gamma::Gamma();
    
    for (auto energy : sourceEnergyValues)
    {
        G4double cross1 = 0.0;
        cross1 += calc.ComputeCrossSectionPerVolume(energy*keV, gamma,"conv",material,0.0);
        cross1 += calc.ComputeCrossSectionPerVolume(energy*keV, gamma,"compt",material,0.0);
        cross1 += calc.ComputeCrossSectionPerVolume(energy*keV, gamma,"phot",material,0.0);
        cross1 += calc.ComputeCrossSectionPerVolume(energy*keV, gamma,"Rayl",material,0.0);
        G4double mu = cross1*cm; // cm^-1
        attenuationSpectrum.push_back(mu);
    }
    // Save to file

    std::ofstream outfile(matName+".txt");
    if (!outfile) {
        std::cerr << "Failed to open output file" << std::endl;
        return;
    }
    outfile << std::scientific; // Enable scientific notation
    outfile << "keV"<<"\t"<<"att"<<std::endl;
    for (size_t i = 0; i < sourceEnergyValues.size(); ++i) {
        outfile << sourceEnergyValues[i] << "\t" << attenuationSpectrum[i] << std::endl;
    }
    outfile.close();
}
