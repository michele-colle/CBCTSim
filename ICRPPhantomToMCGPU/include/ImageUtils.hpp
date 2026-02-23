#ifndef IMAGE_UTILS_HPP
#define IMAGE_UTILS_HPP
#include <itkImageFileReader.h>
#include <itkRegionOfInterestImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include "G4DatReader.hpp"
#include <itkResampleImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkNearestNeighborInterpolateImageFunction.h>
#include <itkConstantPadImageFilter.h>
#include "globals.hh"
class ImageUtils {
public: 

    static G4DatReader::LabelImageType::RegionType GetRegionFromMask(G4DatReader::LabelImageType::Pointer mask);
    using InternalImageType = itk::Image<float, 3>;
    static ImageUtils::InternalImageType::Pointer CreateHUPhantom(G4DatReader::LabelImageType::Pointer organImage, G4double energy, G4DatReader::PhantomSex sex);
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

    static void ImageToRawFile(InternalImageType::Pointer input, std::string savePath);
    static G4DatReader::LabelImageType::Pointer InsertPhysicalVerticalBarella(G4DatReader::LabelImageType::Pointer centeredLabels, 
                           double yStart_mm, double carbonThick_mm, double foamThick_mm);
    static G4DatReader::LabelImageType::Pointer OrganLabelsToMaterialLabels(G4DatReader::LabelImageType::Pointer organLabels);
    static void LabelsToRawFile(G4DatReader::LabelImageType::Pointer image, const std::string& filename);

    static G4DatReader::LabelImageType::Pointer GetSegmentedLabelsFromFullPhantom(
        G4DatReader::LabelImageType::Pointer fullPhantom,
        std::string maskPath);

    // Helper to center-pad an image to a specific target size
    template <typename TImage>
    static typename TImage::Pointer CenterPadToSize(typename TImage::Pointer input, itk::Size<3> targetSize, float constant = 0.0) {
        auto currentSize = input->GetLargestPossibleRegion().GetSize();
        
        itk::Size<3> lowerExtend, upperExtend;
        for (int i = 0; i < 3; i++) {
            int diff = targetSize[i] - currentSize[i];
            if (diff < 0) diff = 0; // Don't crop if it's already bigger
            lowerExtend[i] = diff / 2;
            upperExtend[i] = diff - lowerExtend[i];
        }

        auto padFilter = itk::ConstantPadImageFilter<TImage, TImage>::New();
        padFilter->SetInput(input);
        padFilter->SetPadLowerBound(lowerExtend);
        padFilter->SetPadUpperBound(upperExtend);
        padFilter->SetConstant(constant);
        padFilter->Update();
        // 2. Disconnect the output from the pipeline 
        // This prevents the filter from overwriting metadata later
        typename TImage::Pointer vol = padFilter->GetOutput();
        vol->DisconnectPipeline();

        // 3. Define the origin using ITK's Point type
        typename TImage::PointType newOrigin;
        newOrigin.Fill(0.0);
        
        // 4. Set origin and direction (Direction is often why Slicer looks shifted)
        vol->SetOrigin(newOrigin);
        vol->SetRegions(vol->GetLargestPossibleRegion().GetSize());
        vol->Allocate();
        return vol;
    }
};
#endif

