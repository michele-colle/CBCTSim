#ifndef IMAGE_UTILS_HPP
#define IMAGE_UTILS_HPP
#include <itkImageFileReader.h>
#include <itkRegionOfInterestImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include "G4DatReader.hpp"
#include <itkResampleImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkNearestNeighborInterpolateImageFunction.h>
class ImageUtils {
public: 

    static G4DatReader::LabelImageType::RegionType GetRegionFromMask(G4DatReader::LabelImageType::Pointer mask);
    static void OrganLabelToMaterialLabelRawFile(G4DatReader::LabelImageType::Pointer image, const std::string& filename);
    // Function to resample an image to a new isotropic resolution
    template <typename TImage>
    static typename TImage::Pointer ResampleImage(typename TImage::Pointer input, double newResolution){
        auto resampler = itk::ResampleImageFilter<TImage, TImage>::New();
    
    // 1. Define the new grid
    auto originalSize = input->GetLargestPossibleRegion().GetSize();
    auto originalSpacing = input->GetSpacing();
    
    typename TImage::SpacingType outSpacing;
    outSpacing.Fill(newResolution);
    
    typename TImage::SizeType outSize;
    for(int i=0; i<3; ++i) {
        double physicalDim = originalSize[i] * originalSpacing[i];
        outSize[i] = static_cast<unsigned long>(physicalDim / newResolution);
    }

    resampler->SetInput(input);
    resampler->SetSize(outSize);
    resampler->SetOutputSpacing(outSpacing);
    resampler->SetOutputOrigin(input->GetOrigin());
    resampler->SetOutputDirection(input->GetDirection());
    
    // 2. Use Linear for CT (HU) and NearestNeighbor for Labels (Organ IDs)
    if constexpr (std::is_same_v<typename TImage::PixelType, float>) {
        resampler->SetInterpolator(itk::LinearInterpolateImageFunction<TImage, double>::New());
    } else {
        resampler->SetInterpolator(itk::NearestNeighborInterpolateImageFunction<TImage, double>::New());
    }

    resampler->Update();
    return resampler->GetOutput();
    };

    static G4DatReader::LabelImageType::Pointer GetSegmentedLabelsFromFullPhantom(
        G4DatReader::LabelImageType::Pointer fullPhantom,
        std::string maskPath);
};
#endif

