#ifndef IMAGE_UTILS_HPP
#define IMAGE_UTILS_HPP
#include <itkImageFileReader.h>
#include <itkRegionOfInterestImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include "G4DatReader.hpp"
class ImageUtils {
public:
    static G4DatReader::LabelImageType::RegionType GetRegionFromMask(G4DatReader::LabelImageType::Pointer mask);
    static void OrganLabelToMaterialLabelRawFile(G4DatReader::LabelImageType::Pointer image, const std::string& filename);
};
#endif