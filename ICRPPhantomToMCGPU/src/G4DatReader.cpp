#include "G4DatReader.hpp"
#include <itkDirectory.h>
#include <fstream>
#include <algorithm>
#include <limits>

// Helper for natural sorting (slice1, slice2...)
bool naturalSort(const std::string& a, const std::string& b) {
    auto extractNum = [](const std::string& s) {
        std::string n;
        for(char c : s) if(std::isdigit(c)) n+=c;
        return n.empty() ? 0 : std::stoi(n);
    };
    return extractNum(a) < extractNum(b);
}
// --- Helper function to simulate "ls" ---
void ListDirectory(const std::string& path) {
    auto directory = itk::Directory::New();
    std::cout << "\n[DEBUG ls] Listing contents of: " << path << std::endl;
    
    if (directory->Load(path.c_str())) {
        size_t fileCount = directory->GetNumberOfFiles();
        if (fileCount == 0) {
            std::cout << "  (Directory is empty)" << std::endl;
        }
        for (size_t i = 0; i < fileCount; ++i) {
            std::cout << "  - " << directory->GetFile(i) << std::endl;
        }
    } else {
        std::cout << "  [ERROR] Could not open/find directory!" << std::endl;
    }
    std::cout << "------------------------------------------\n" << std::endl;
}
G4DatReader::LabelImageType::Pointer G4DatReader::LoadPhantom(const std::string& basePath, PhantomSex sex) {
    std::string dataFilePath = basePath + (sex == PhantomSex::Male ? "/MaleData.dat" : "/FemaleData.dat");
    std::string slicesDir = basePath + "/ICRP110_g4dat" + (sex == PhantomSex::Male ? "/AM/" : "/AF/");

    //ListDirectory(basePath);
    // 1. Parse Metadata Header
    std::ifstream master(dataFilePath);
    if (!master.is_open()) {
        std::cerr << "ERROR: Could not open master file: " << dataFilePath << std::endl;
        return nullptr;
    }
    Metadata meta;
    master >> meta.nz >> meta.nx >> meta.ny >> meta.dx >> meta.dy >> meta.dz >> meta.numMaterials;

    // 2. Initialize ITK Image
    auto image = LabelImageType::New();
    LabelImageType::RegionType region;
    LabelImageType::SizeType size = {{ (size_t)meta.nx, (size_t)meta.ny, (size_t)meta.nz }};
    region.SetSize(size);
    image->SetRegions(region);
    image->Allocate();
    
    double spacing[3] = {meta.dx, meta.dy, meta.dz};
    image->SetSpacing(spacing);

    // 3. Find and sort .g4dat files
    auto itkDir = itk::Directory::New();
    itkDir->Load(slicesDir.c_str());
    std::vector<std::string> sliceFiles;
    for(size_t i=0; i<itkDir->GetNumberOfFiles(); ++i) {
        std::string f = itkDir->GetFile(i);
        if(f.find(".g4dat") != std::string::npos) sliceFiles.push_back(slicesDir + f);
    }
    std::sort(sliceFiles.begin(), sliceFiles.end(), naturalSort);

    // 4. Load Voxel Data
    std::cout << "Loading " << sliceFiles.size() << " slices into 3D volume..." << std::endl;
    for (int z = 0; z < meta.nz; ++z) {
        std::ifstream sliceFile(sliceFiles[z]);
        int junkX, junkY, junkExtra;
        sliceFile >> junkX >> junkY >> junkExtra; // Skip slice header

        for (int y = 0; y < meta.ny; ++y) {
            for (int x = 0; x < meta.nx; ++x) {
                int orgID;
                sliceFile >> orgID;
                LabelImageType::IndexType idx = {{x, y, z}};
                image->SetPixel(idx, static_cast<unsigned short>(orgID));
            }
        }
    }

    return image;
}

int G4DatReader::MapOrganToMaterial(int OrgID){

    int mateID_out = 0;
    // The code below associates organ ID numbers (called here mateID) from ASCII slice
    // files with material ID numbers (called here mateID_out) as defined in ICRP110PhantomMaterials
    // Material and Organ IDs are associated as stated in AM_organs.dat and FM_organs.dat depending on
    // the sex of the phantom (male and female, respctively)

    if (OrgID == 128)
    {
    mateID_out = 1;
    }

    else if (OrgID == 13 || OrgID == 16 || OrgID == 19 || OrgID == 22 || OrgID == 24 || OrgID == 26 || OrgID == 28 || OrgID == 31 || OrgID == 34 || OrgID == 37 || OrgID == 39 || OrgID == 41 || OrgID == 43 || OrgID == 45 || OrgID == 47 || OrgID == 49 || OrgID == 51 || OrgID == 53 || OrgID == 55)
    {
    mateID_out = 2;
    }

    else if (OrgID == 14)
    {
    mateID_out = 3;
    }

    else if (OrgID == 17)
    {
    mateID_out = 4;
    }

    else if (OrgID == 20)
    {
    mateID_out = 5;
    }

    else if (OrgID == 23)
    {
    mateID_out = 6;
    }

    else if (OrgID == 25)
    {
    mateID_out = 7;
    }

    else if (OrgID == 27)
    {
    mateID_out = 8;
    }

    else if (OrgID == 29)
    {
    mateID_out = 9;
    }

    else if (OrgID == 32)
    {
    mateID_out = 10;
    }

    else if (OrgID == 35)
    {
    mateID_out = 11;
    }

    else if (OrgID == 38)
    {
    mateID_out = 12;
    }

    else if (OrgID == 40)
    {
    mateID_out = 13;
    }

    else if (OrgID == 42)
    {
    mateID_out = 14;
    }

    else if (OrgID == 44)
    {
    mateID_out = 15;
    }

    else if (OrgID == 46)
    {
    mateID_out = 16;
    }

    else if (OrgID == 48)
    {
    mateID_out = 17;
    }

    else if (OrgID == 50)
    {
    mateID_out = 18;
    }

    else if (OrgID == 52)
    {
    mateID_out = 19;
    }

    else if (OrgID == 54)
    {
    mateID_out = 20;
    }

    else if (OrgID == 56)
    {
    mateID_out = 21;
    }

    else if (OrgID == 15 || OrgID == 30)
    {
    mateID_out = 22;
    }

    else if (OrgID == 18 || OrgID == 33)
    {
    mateID_out = 23;
    }

    else if (OrgID == 21)
    {
    mateID_out = 24;
    }

    else if (OrgID == 36)
    {
    mateID_out = 25;
    }

    else if (OrgID == 57 || OrgID == 58 || OrgID == 59 || OrgID == 60)
    {
    mateID_out = 26;
    }

    else if (OrgID == 122 || OrgID == 123 || OrgID == 124 || OrgID == 125 || OrgID == 141)
    {
    mateID_out = 27;
    }

    else if (OrgID == 9 || OrgID == 10 || OrgID == 11 || OrgID == 12 || OrgID == 88 || OrgID == 96 || OrgID == 98)
    {
    mateID_out = 28;
    }

    else if (OrgID == 5 || OrgID == 6 || OrgID == 106 || OrgID == 107 || OrgID == 108 || OrgID == 109 || OrgID == 133)
    {
    mateID_out = 29;
    }

    else if (OrgID == 95)
    {
    mateID_out = 30;
    }

    else if (OrgID == 113)
    {
    mateID_out = 31;
    }

    else if (OrgID == 61)
    {
    mateID_out = 32;
    }

    else if (OrgID == 87)
    {
    mateID_out = 33;
    }

    else if (OrgID == 66 || OrgID == 67 || OrgID == 68 || OrgID == 69)
    {
    mateID_out = 34;
    }

    else if (OrgID == 89 || OrgID == 90 || OrgID == 91 || OrgID == 92 || OrgID == 93 || OrgID == 94)
    {
    mateID_out = 35;
    }

    else if (OrgID == 72)
    {
    mateID_out = 36;
    }

    else if (OrgID == 74)
    {
    mateID_out = 37;
    }

    else if (OrgID == 76 || OrgID == 78 || OrgID == 80 || OrgID == 82 || OrgID == 84 || OrgID == 86)
    {
    mateID_out = 38;
    }

    else if (OrgID == 127)
    {
    mateID_out = 39;
    }

    else if (OrgID == 132)
    {
    mateID_out = 40;
    }

    else if (OrgID == 137)
    {
    mateID_out = 41;
    }

    else if (OrgID == 111 || OrgID == 112 || OrgID == 129 || OrgID == 130)
    {
    mateID_out = 42;
    }

    else if (OrgID == 1 || OrgID == 2)
    {
    mateID_out = 43;
    }

    else if (OrgID == 110)
    {
    mateID_out = 44;
    }

    else if (OrgID == 3 || OrgID == 4 || OrgID == 7 || OrgID == 8 || OrgID == 70 || OrgID == 71 || OrgID == 114 || OrgID == 120 || OrgID == 121 || OrgID == 126 || OrgID == 131 || OrgID == 134 || OrgID == 135 || OrgID == 136)
    {
    mateID_out = 45;
    }

    else if (OrgID == 115 || OrgID == 139)
    {
    mateID_out = 46;
    }

    else if (OrgID == 100 || OrgID == 101 || OrgID == 102 || OrgID == 103 || OrgID == 104 || OrgID == 105)
    {
    mateID_out = 47;
    }

    else if (OrgID == 63 || OrgID == 65)
    {
    mateID_out = 48;
    }

    else if (OrgID == 62 || OrgID == 64 || OrgID == 116 || OrgID == 117 || OrgID == 118 || OrgID == 119)
    {
    mateID_out = 49;
    }

    else if (OrgID == 97 || OrgID == 99)
    {
    mateID_out = 50;
    }

    else if (OrgID == 73 || OrgID == 75 || OrgID == 77 || OrgID == 79 || OrgID == 81 || OrgID == 83 || OrgID == 85)
    {
    mateID_out = 51;
    }

    else if (OrgID == 138)
    {
    mateID_out = 52;
    }

    else if (OrgID == 0 || OrgID == 140)
    {
    mateID_out = 0;
    }

    else
    {
    mateID_out = OrgID;
    }
    return mateID_out;
}