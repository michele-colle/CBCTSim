#ifndef G4DATEREADER_HPP
#define G4DATEREADER_HPP 

#include <itkImage.h>
#include <string>
#include <vector>

class G4DatReader {
    public:
        enum PhantomSex{Male, Female};
        struct Metadata {
            int nx, ny, nz;
            double dx, dy, dz; // Spacing
            int numMaterials;
        };
        using LabelImageType = itk::Image<unsigned short, 3>; // For Raw IDs
        static LabelImageType::Pointer LoadPhantom(const std::string& basePath,PhantomSex sex); 
        static int MapOrganToMaterial(int OrgID);
};
#endif