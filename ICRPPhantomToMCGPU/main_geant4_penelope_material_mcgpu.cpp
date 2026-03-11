#include "ICRP110PhantomConstruction.hh"
#include "ICRP110PhantomNestedParameterisation.hh"
#include "ICRP110PhantomMaterial_Female.hh"
#include "G4SystemOfUnits.hh"
#include "ICRP110PhantomMaterial_Male.hh"
#include "G4NistManager.hh"
#include <array>
#include <string>
#include <iostream>
#include <filesystem>
#include <vector>
// Total count: 53 materials
std::array<std::string, 53> materialNames = {
    "air",               // [00] 
    "teeth",             // [01] 
    "bone",              // [02] 
    "humeri_upper",      // [03] 
    "humeri_lower",      // [04] 
    "arm_lower",         // [05] 
    "hand",              // [06] 
    "clavicle",          // [07] 
    "cranium",           // [08] 
    "femora_upper",      // [09] 
    "femora_lower",      // [10] 
    "leg_lower",         // [11] 
    "foot",              // [12] 
    "mandible",          // [13] 
    "pelvis",            // [14] 
    "ribs",              // [15] 
    "scapulae",          // [16] 
    "spine_cervical",    // [17] 
    "spine_thoratic",    // [18] 
    "spine_lumbar",      // [19] 
    "sacrum",            // [20] 
    "sternum",           // [21] 
    "hf_upper",          // [22] 
    "hf_lower",          // [23] 
    "med_lowerarm",      // [24] 
    "med_lowerleg",      // [25] 
    "cartilage",         // [26] 
    "skin",              // [27] 
    "blood",             // [28] 
    "muscle",            // [29] 
    "liver",             // [30] 
    "pancreas",          // [31] 
    "brain",             // [32] 
    "heart",             // [33] 
    "eye",               // [34] 
    "kidney",            // [35] 
    "stomach",           // [36] 
    "intestine_sml",     // [37] 
    "intestine_lrg",     // [38] 
    "spleen",            // [39] 
    "thyroid",           // [40] 
    "bladder",           // [41] 
    "ovaries_testes",    // [42] 
    "adrenals",          // [43] 
    "oesophagus",        // [44] 
    "misc",              // [45] 
    "uterus_prostate",   // [46] 
    "lymph",             // [47] 
    "breast_glandular",  // [48] 
    "breast_adipose",    // [49] 
    "lung",              // [50] 
    "gastro_content",    // [51] 
    "urine"              // [52] 
};
void PenelopeMatToMCGPU( const std::string materialFilePath,
    const std::string mcgpuFilePath){
    std::stringstream ss;
    ss<<5000<<" "<<120005<<std::endl;
    ss<<23002<<std::endl;
    ss<<materialFilePath<<std::endl;
    ss<<mcgpuFilePath<<std::endl;

    

    std::string fullCommand = "printf \"" + ss.str() + "\" | ./create_material_debug_rodrigo.x";
    
    std::cout<<std::endl<<std::endl;
    std::cout<<fullCommand.c_str();
    std::cout<<std::endl<<std::endl;
    int result = std::system(fullCommand.c_str());
    if (result != 0){
        std::cerr << "ERROR: Failed to create " <<mcgpuFilePath<<  std::endl;
    }
}
std::string G4MaterialToPenelopeFile(G4Material* material)
{
  
    std::stringstream ss;
    ss<<1<<std::endl;
    ss<<material->GetName()<<std::endl;
    ss<<material->GetNumberOfElements()<<std::endl;
    ss<<2<<std::endl;
    const G4ElementVector* elements = material->GetElementVector();
    const G4double* massFractions = material->GetFractionVector();
    for(size_t i=0; i<material->GetNumberOfElements(); ++i)
    {
        const G4Element* element = (*elements)[i];
        G4double fraction = massFractions[i];
        ss<<element->GetZ()<<" "<<fraction<<std::endl;
    }
    ss<<2<<std::endl;
    ss<<material->GetDensity()/ (g/cm3)<<std::endl;
    ss<<2<<std::endl;
    std::string materialFilePath = material->GetName() + ".mat";
    ss<<materialFilePath<<std::endl;

    // std::cout<<std::endl<<std::endl;
    // std::cout<<ss.str();
    // std::cout<<std::endl<<std::endl;


    
    std::string fullCommand = "printf \"" + ss.str() + "\" | ./material.x";
    int result = std::system(fullCommand.c_str());
    if (result != 0){
        std::cerr << "ERROR: Failed to create " <<materialFilePath<<  std::endl;
    }
    ss.str("");
    ss.clear();

    return materialFilePath;

}
std::vector<std::string> G4MaterialToPenelopeFile(const std::vector<G4Material*>& materials){
    std::vector<std::string> penelopeMaterialFiles;
    for(size_t i = 0; i < materials.size(); ++i)
    {   
        G4Material* material = materials[i];
        std::cout << "Index: " << i << " | Material: " << material->GetName() << std::endl;

        std::string outFileName;
        if(material) {
            outFileName=G4MaterialToPenelopeFile(material);
            penelopeMaterialFiles.push_back(outFileName);
        }
    }
    return penelopeMaterialFiles;
}

// std::string CreateMCGPUMaterialFiles(G4Material* material,const std::string target_folder_relative)
// {
  
//     std::stringstream ss;
//     ss<<1<<std::endl;
//     ss<<material->GetName()<<std::endl;
//     ss<<material->GetNumberOfElements()<<std::endl;
//     ss<<2<<std::endl;
//     const G4ElementVector* elements = material->GetElementVector();
//     const G4double* massFractions = material->GetFractionVector();
//     for(size_t i=0; i<material->GetNumberOfElements(); ++i)
//     {
//         const G4Element* element = (*elements)[i];
//         G4double fraction = massFractions[i];
//         ss<<element->GetZ()<<" "<<fraction<<std::endl;
//     }
//     ss<<2<<std::endl;
//     ss<<material->GetDensity()/ (g/cm3)<<std::endl;
//     ss<<2<<std::endl;
//     std::string materialFilePath = material->GetName() + ".mat";
//     ss<<materialFilePath<<std::endl;

//     // std::cout<<std::endl<<std::endl;
//     // std::cout<<ss.str();
//     // std::cout<<std::endl<<std::endl;


    
//     std::string fullCommand = "printf \"" + ss.str() + "\" | ./material.x";
//     int result = std::system(fullCommand.c_str());
//     if (result != 0){
//         std::cerr << "ERROR: Failed to create " <<materialFilePath<<  std::endl;
//     }
//     ss.str("");
//     ss.clear();

//     std::string mcgpuFilePath = target_folder_relative + "icrp_" + material->GetName() + ".mcgpu";
//     return PenelopeMatToMCGPU(mcgpuFilePath,materialFilePath);

// }
double get_penelope_density(const std::string& filePath) {
    std::ifstream file(filePath);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filePath << std::endl;
        return -1.0;
    }

    while (std::getline(file, line)) {
        // Look for the line containing the density
        if (line.find("Mass density") != std::string::npos) {
            // The line looks like: "Mass density = 1.03000000E+00 g/cm**3"
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string valueStr = line.substr(eqPos + 1);
                std::stringstream ss(valueStr);
                double density;
                ss >> density; // Stringstream handles scientific notation (E+00) automatically
                return density;
            }
        }
    }

    return -1.0; // Not found
}
void PenelopeFilesToMCGPUFiles(const std::vector<std::string>& penelopeMaterialFiles,const std::string target_folder_relative)
{
    //creo la lista per collegare Id e file
    std::filesystem::create_directories(target_folder_relative);
    std::vector<std::string> materialFileIdList;
    materialFileIdList.push_back("material/icrp_air.mcgpu.mcgpu density=0.0012 voxelId=0");
    for(size_t i = 0; i < penelopeMaterialFiles.size(); ++i)
    {   
        double density = get_penelope_density(penelopeMaterialFiles[i]);
        std::filesystem::path p = penelopeMaterialFiles[i];
        std::string base_name = p.stem().string();
        std::string mcgpuFilePath = target_folder_relative + "icrp_" + base_name + ".mcgpu";

        PenelopeMatToMCGPU(penelopeMaterialFiles[i],mcgpuFilePath);
        materialFileIdList.push_back("material/"+mcgpuFilePath+" "+
            "density="+std::to_string(density)+" voxelId="+std::to_string(i+1));
    }
    std::string outputFileName = target_folder_relative+"MC-GPU_material_config.txt";
    std::ofstream outFile(outputFileName);

    if (outFile.is_open()) {
        for (const auto& entry : materialFileIdList) {
            outFile << entry << "\n";
        }
        outFile.close();
        std::cout << "Successfully wrote material list to " << outputFileName << std::endl;
    }
    else {
        std::cerr << "Error: Could not open file for writing!" << std::endl;
    }
}
// void G4MaterialVectorToMCGPUFiles(const std::vector<G4Material*>& materials,const std::string target_folder_relative)
// {
//     //creo la lista per collegare Id e file
//     std::vector<std::string> materialFileIdList;
//     materialFileIdList.push_back("material/air__5-120keV.mcgpu density=0.0012 voxelId=0");
//     for(size_t i = 1; i < materials.size(); ++i)
//     {   
//         G4Material* material = materials[i];
//         std::cout << "Index: " << i << " | Material: " << material->GetName() << std::endl;

//         std::string outFileName;
//         if(material) {
//             outFileName=CreateMCGPUMaterialFiles(material,target_folder_relative);
//         }
//         materialFileIdList.push_back("material/"+outFileName+" "+
//             "density="+std::to_string(material->GetDensity()/ (g/cm3))+" voxelId="+std::to_string(i+1));
//     }

//     std::string outputFileName = target_folder_relative+"MC-GPU_material_config.txt";
//     std::ofstream outFile(outputFileName);

//     if (outFile.is_open()) {
//         for (const auto& entry : materialFileIdList) {
//             outFile << entry << "\n";
//         }
//         outFile.close();
//         std::cout << "Successfully wrote material list to " << outputFileName << std::endl;
//     }
//     else {
//         std::cerr << "Error: Could not open file for writing!" << std::endl;
//     }
// }
std::vector<G4Material*> GetMaterialsMale()
{
    auto fMaterial_Male = new ICRP110PhantomMaterial_Male();
    fMaterial_Male -> DefineMaterials();

    std::vector<G4Material*> materials;
    for(size_t i = 1; i < materialNames.size(); ++i)
    {
        const std::string& name = materialNames[i];
        G4Material* material = fMaterial_Male->GetMaterial(name);
        if(material) 
            materials.push_back(material);
    }
    return materials;
}
std::vector<G4Material*> GetMaterialsFemale()
{
    auto fMaterial_Female = new ICRP110PhantomMaterial_Female();
    fMaterial_Female -> DefineMaterials();

    std::vector<G4Material*> materials;
    for(size_t i = 1; i < materialNames.size(); ++i)
    {
        const std::string& name = materialNames[i];
        G4Material* material = fMaterial_Female->GetMaterial(name);
        if(material) 
            materials.push_back(material);
    }
    return materials;
}
G4Material* GetPolyurethaneFoam(){
    //https://www.osti.gov/biblio/1782721#:~:text=Abstract,for%20properties%20of%20372%20materials.
    G4NistManager* nist = G4NistManager::Instance();

    // 1. Get the elements from NIST (this ensures high-precision atomic masses)
    G4Element* elH = nist->FindOrBuildElement("H");
    G4Element* elC = nist->FindOrBuildElement("C");
    G4Element* elN = nist->FindOrBuildElement("N");
    G4Element* elO = nist->FindOrBuildElement("O");

    // 2. Define the Rigid Polyurethane Material
    // Typical density for rigid expanded foam: 0.03 to 0.1 g/cm3
    G4double density = 0.021 * g/cm3; 
    G4Material* PU_foam = new G4Material("polyufoam", density, 4);

    // 3. Add elements by Weight Fraction (NIST standard)
    PU_foam->AddElement(elH,  0.041001);
    PU_foam->AddElement(elN,  0.121001);
    PU_foam->AddElement(elC, 0.543998);
    PU_foam->AddElement(elO, 0.294000);
    return PU_foam;
}
int main()
{
    //cartella pendb, partendo dalla cartella build di questo progetto
    //i due eseguibili dentro pendb sono material.x (per creare il .mat di penelope)
    // e create_material_debug_rodrigo.x per creare il file .mcgpu
    std::string penelopeDir = "../../../penelope/pendbase/";

    //mi sposto in pendb
    std::system("ls");
    std::filesystem::current_path(penelopeDir);
    std::system("ls");

    PenelopeFilesToMCGPUFiles(G4MaterialToPenelopeFile(GetMaterialsFemale()),"G4_ICRP_female/");
    PenelopeFilesToMCGPUFiles(G4MaterialToPenelopeFile(GetMaterialsMale()),"G4_ICRP_male/");
    
    auto polyAddress = G4MaterialToPenelopeFile(std::vector<G4Material*>{GetPolyurethaneFoam()})[0];
    std::vector<std::string> penFileList = {"water.mat",
                                            "al.mat",
                                            "pmma.mat",
                                            "pb.mat",
                                            "CsI.mat",
                                            "c.mat",
                                            "air.mat",
                                            polyAddress};
    PenelopeFilesToMCGPUFiles(penFileList,"Pen_base/");

    return 0;
}