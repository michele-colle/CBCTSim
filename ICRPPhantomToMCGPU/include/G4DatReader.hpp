#ifndef G4DATEREADER_HPP
#define G4DATEREADER_HPP 

#include <itkImage.h>
#include <string>
#include <vector>
#include <itkDirectory.h>
#include <fstream>

class G4DatReader {
    public:
    
        enum PhantomSex{Male, Female};
        struct Metadata {
            int nx, ny, nz;
            double dx, dy, dz; // Spacing
            int numMaterials;
        };
        static int MapOrganToMaterial(int OrgID);
        static float MapMaterialToHU(int materialID);
        // Helper for natural sorting (slice1, slice2...)
        static bool naturalSort(const std::string& a, const std::string& b) {
            auto extractNum = [](const std::string& s) {
                std::string n;
                for(char c : s) if(std::isdigit(c)) n+=c;
                return n.empty() ? 0 : std::stoi(n);
            };
            return extractNum(a) < extractNum(b);
        }
        using LabelImageType = itk::Image<unsigned short, 3>; // For Raw IDs
        static LabelImageType::Pointer LoadPhantom(const std::string& basePath,PhantomSex sex){
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
            meta.dx *= 2.0; // Convert from mm to cm
            meta.dy *= 2.0;
            meta.dz *= 2.0;

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
        };
};
#endif