// mcrp_to_mcgpu_materials.cpp
//
// Reads an MRCP tetrahedral phantom via TETModelImport, extracts the unique
// G4Materials, converts them to Penelope .mat files and then to MC-GPU .mcgpu
// files, and writes the MC-GPU material config list.
//
// Usage:
//   mcrp_to_mcgpu_materials [phantomName] [penelopeDir] [outputDir]
//
// Defaults:
//   phantomName  = MRCP-00F
//   penelopeDir  = ../../../penelope/pendbase/
//   outputDir    = MCRP_female_materials/

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include "TETModelImport.hh"

#include "G4Material.hh"
#include "G4SystemOfUnits.hh"
#include "G4UIExecutive.hh"

// -----------------------------------------------------------------------------
// Helper: shorten a material name to ≤16 chars for Fortran's CHARACTER buffer.
// Step 1: strip non-alphanumeric chars and vowels.
// Step 2 (fallback): take the first char of each '_'/punctuation-separated word.
// -----------------------------------------------------------------------------
static std::string makeSafeMatName(const std::string& name)
{
    static const std::string vowels = "aeiouAEIOU";

    std::string noVowels;
    for (char c : name) {
        if (!std::isalnum((unsigned char)c)) continue;
        if (vowels.find(c) != std::string::npos) continue;
        noVowels += c;
    }
    if (noVowels.size() <= 16) return noVowels;

    std::string initials;
    bool newWord = true;
    for (char c : name) {
        if (!std::isalnum((unsigned char)c)) {
            newWord = true;
        } else if (newWord) {
            initials += c;
            newWord = false;
        }
    }
    if (initials.size() <= 16) return initials;
    return initials.substr(0, 16);
}

// -----------------------------------------------------------------------------
// Helper: write a Penelope material input file and run material.x
// Returns the path of the generated .mat file, or "" on failure.
// -----------------------------------------------------------------------------
static std::string G4MaterialToPenelopeFile(G4Material* material)
{
    std::string materialFilePath = makeSafeMatName(material->GetName()) + ".mat";

    std::stringstream ss;
    ss << 1 << std::endl;
    ss << material->GetName() << std::endl;
    ss << material->GetNumberOfElements() << std::endl;
    ss << 2 << std::endl;

    const G4ElementVector* elements     = material->GetElementVector();
    const G4double*        massFractions = material->GetFractionVector();
    for (size_t i = 0; i < material->GetNumberOfElements(); ++i) {
        const G4Element* element = (*elements)[i];
        G4double         fraction = massFractions[i];
        ss << element->GetZ() << " " << fraction << std::endl;
    }
    ss << 2 << std::endl;
    ss << material->GetDensity() / (g / cm3) << std::endl;
    ss << 2 << std::endl;
    ss << materialFilePath << std::endl;

    std::string fullCommand = "printf \"" + ss.str() + "\" | ./material.x";
    int result = std::system(fullCommand.c_str());
    if (result != 0) {
        std::cerr << "ERROR: material.x failed for " << material->GetName() << std::endl;
        return "";
    }

    return materialFilePath;
}

// -----------------------------------------------------------------------------
// Helper: run create_material_debug_rodrigo.x to convert .mat -> .mcgpu
// -----------------------------------------------------------------------------
static void PenelopeMatToMCGPU(const std::string& materialFilePath,
                                const std::string& mcgpuFilePath)
{
    std::stringstream ss;
    ss << 5000 << " " << 120005 << std::endl;
    ss << 23002 << std::endl;
    ss << materialFilePath << std::endl;
    ss << mcgpuFilePath << std::endl;

    std::string fullCommand =
        "printf \"" + ss.str() + "\" | ./create_material_debug_rodrigo.x";

    std::cout << "\n" << fullCommand << "\n";
    int result = std::system(fullCommand.c_str());
    if (result != 0) {
        std::cerr << "ERROR: create_material_debug_rodrigo.x failed for "
                  << mcgpuFilePath << std::endl;
    }
}

// -----------------------------------------------------------------------------
// Helper: read the density written inside a Penelope .mat file
// -----------------------------------------------------------------------------
static double GetPenelopeDensity(const std::string& filePath)
{
    std::ifstream file(filePath);
    std::string   line;
    if (!file.is_open()) {
        std::cerr << "Could not open: " << filePath << std::endl;
        return -1.0;
    }
    while (std::getline(file, line)) {
        if (line.find("Mass density") != std::string::npos) {
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::stringstream ss(line.substr(eqPos + 1));
                double density;
                ss >> density;
                return density;
            }
        }
    }
    return -1.0;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv)
{
    // -------------------------------------------------------------------------
    // 1. Parse arguments
    // -------------------------------------------------------------------------
    const char* home = std::getenv("HOME");
    std::string phantomName  = "MRCP-00F";
    std::string penelopeDir  = home ? std::string(home) + "/penelope/pendbase/" : "/penelope/pendbase/";

    if (argc > 1) phantomName = argv[1];
    if (argc > 2) penelopeDir = argv[2];

    std::string outputDir = phantomName + "_materials/";
    if (argc > 3) outputDir = argv[3];

    std::cout << "Phantom      : " << phantomName  << std::endl;
    std::cout << "Penelope dir : " << penelopeDir  << std::endl;
    std::cout << "Output dir   : " << outputDir    << std::endl;

    // -------------------------------------------------------------------------
    // 2. Load the MRCP phantom
    // -------------------------------------------------------------------------
    G4UIExecutive* ui = nullptr;
    TETModelImport tetImport(phantomName, ui, "./phantoms");

    const G4int numTet = tetImport.GetNumTetrahedron();
    std::cout << "Total tetrahedra: " << numTet << std::endl;

    // -------------------------------------------------------------------------
    // 3. Collect unique material IDs (organ IDs) from all tetrahedra
    // -------------------------------------------------------------------------
    std::set<G4int> uniqueMatIDs;
    for (G4int t = 0; t < numTet; ++t)
        uniqueMatIDs.insert(tetImport.GetMaterialIndex(t));

    std::cout << "Unique material IDs found: " << uniqueMatIDs.size() << std::endl;

    // Build ordered map: matID -> G4Material*
    std::map<G4int, G4Material*> matIDToMaterial;
    for (G4int id : uniqueMatIDs) {
        G4Material* mat = tetImport.GetMaterial(id);
        if (mat)
            matIDToMaterial[id] = mat;
        else
            std::cerr << "WARNING: no G4Material for ID " << id << std::endl;
    }

    // -------------------------------------------------------------------------
    // 4. Change working directory to penelope pendbase
    //    (material.x and create_material_debug_rodrigo.x must be run from there)
    // -------------------------------------------------------------------------
    std::filesystem::create_directories(penelopeDir);
    std::filesystem::current_path(penelopeDir);
    std::filesystem::create_directories(outputDir);

    std::cout << "Working directory: " << std::filesystem::current_path() << std::endl;

    // -------------------------------------------------------------------------
    // 5. For each material: .mat -> .mcgpu, collect config lines
    // -------------------------------------------------------------------------
    // Config line format expected by MC-GPU:
    //   <path>.mcgpu  density=<g/cm3>  voxelId=<organID>
    std::vector<std::string> configLines;

    // voxelId=0 is always air (background)
    configLines.push_back("material/icrp_air.mcgpu density=0.0012 voxelId=0");

    for (auto& [matID, material] : matIDToMaterial) {
        std::cout << "\n--- Processing material ID " << matID
                  << " : " << material->GetName() << " ---" << std::endl;

        // Step A: G4Material -> Penelope .mat
        std::string matFile = G4MaterialToPenelopeFile(material);
        if (matFile.empty()) continue;

        // Step B: .mat -> .mcgpu
        std::string mcgpuFile = outputDir + "icrp_" + material->GetName() + ".mcgpu";
        PenelopeMatToMCGPU(matFile, mcgpuFile);

        // Step C: read density from the generated .mat for the config
        double density = GetPenelopeDensity(matFile);
        if (density < 0.0)
            density = material->GetDensity() / (g / cm3);  // fallback

        configLines.push_back("material/" + mcgpuFile +
                               " density=" + std::to_string(density) +
                               " voxelId=" + std::to_string(matID));
    }

    // -------------------------------------------------------------------------
    // 6. Write MC-GPU material config file
    // -------------------------------------------------------------------------
    std::string configPath = outputDir + "MC-GPU_material_config.txt";
    std::ofstream outFile(configPath);
    if (!outFile.is_open()) {
        std::cerr << "ERROR: cannot write " << configPath << std::endl;
        return 1;
    }
    for (const auto& line : configLines)
        outFile << line << "\n";
    outFile.close();

    std::cout << "\nDone. Material config written to: " << configPath << std::endl;
    return 0;
}
