#include "ImageUtils.hpp"
#include <iomanip> // For std::setprecision
// Function to get the cropping region from a binary mask
G4DatReader::LabelImageType::RegionType ImageUtils::GetRegionFromMask(G4DatReader::LabelImageType::Pointer mask) {
    using IteratorType = itk::ImageRegionConstIterator<G4DatReader::LabelImageType>;
    IteratorType it(mask, mask->GetLargestPossibleRegion());

    G4DatReader::LabelImageType::IndexType minIdx, maxIdx;
    minIdx.Fill(std::numeric_limits<long>::max());
    maxIdx.Fill(std::numeric_limits<long>::min());

    bool foundAny = false;
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() > 0) { // If it's part of your VOI
            G4DatReader::LabelImageType::IndexType currentIndex = it.GetIndex();
            for (int i = 0; i < 3; ++i) {
                if (currentIndex[i] < minIdx[i]) minIdx[i] = currentIndex[i];
                if (currentIndex[i] > maxIdx[i]) maxIdx[i] = currentIndex[i];
            }
            foundAny = true;
        }
    }

    if (!foundAny) {
        throw std::runtime_error("VOI Mask is empty! No '1's found.");
    }

    G4DatReader::LabelImageType::SizeType size;
    for (int i = 0; i < 3; ++i) {
        size[i] = (maxIdx[i] - minIdx[i]) + 1;
    }

    return G4DatReader::LabelImageType::RegionType(minIdx, size);
}

void ImageUtils::OrganLabelToMaterialLabelRawFile(G4DatReader::LabelImageType::Pointer finalImage, const std::string& filename){
    auto spacing = finalImage->GetSpacing();
    auto region = finalImage->GetLargestPossibleRegion();
    auto size = region.GetSize();
    itk::Vector<double, 3> origin;
    for (int i = 0; i < 3; ++i) {
        origin[i] = -static_cast<double>(size[i]) * spacing[i]/2.0;
    }

    // 1. Prepare the Raw file (8-bit unsigned)
    std::string rawPath = filename + 
                      std::to_string(size[0]) + "x" + 
                      std::to_string(size[1]) + "x" + 
                      std::to_string(size[2]) + ".raw";
    std::ofstream rawFile(rawPath, std::ios::binary);

    // 2. Iterate through the volume
    itk::ImageRegionConstIterator<G4DatReader::LabelImageType> it(finalImage, region);

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        int organID = static_cast<int>(it.Get());
        
        // Convert Organ ID -> Material ID (0-52)
        unsigned char materialID = static_cast<unsigned char>(G4DatReader::MapOrganToMaterial(organID));
        
        // Write 1 byte to the raw file
        rawFile.write(reinterpret_cast<char*>(&materialID), sizeof(unsigned char));
    }
    rawFile.close();

// 2. Open the info file
std::ofstream infoFile(filename + ".txt");

// Set precision for floats to ensure the decimal points look right
infoFile << std::fixed << std::setprecision(3);

infoFile << "#[SECTION VOXELIZED GEOMETRY FILE v.2017-07-26]\n";
infoFile << rawPath << "     # VOXEL GEOMETRY FILE (penEasy 2008 format; .gz accepted)\n";

// Convert origin mm -> cm
infoFile << " " << (origin[0] / 10.0) << "  " 
         << (origin[1] / 10.0) << "  " 
         << (origin[2] / 10.0) 
         << "              # OFFSET OF THE VOXEL GEOMETRY [cm]\n";

// Number of voxels
infoFile << " " << size[0] << " " << size[1] << " " << size[2] 
         << "                 # NUMBER OF VOXELS\n";

// Convert spacing mm -> cm
infoFile << " " << (spacing[0] / 10.0) << " " 
         << (spacing[1] / 10.0) << " " 
         << (spacing[2] / 10.0) 
         << "           # VOXEL SIZES [cm]\n";

// Binary tree settings (usually 1 1 1 or 0 0 0 for raw)
infoFile << " 1 1 1                          # SIZE OF LOW RESOLUTION VOXELS\n";

infoFile.close();

std::cout << "MCGPU info file generated: " << filename << ".info" << std::endl;
}
