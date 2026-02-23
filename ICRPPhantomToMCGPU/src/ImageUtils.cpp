#include "ImageUtils.hpp"
#include <iomanip> // For std::setprecision
#include <itkResampleImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkNearestNeighborInterpolateImageFunction.h>
#include <itkRegionOfInterestImageFilter.h>
#include <itkMaskImageFilter.h>
#include "G4MaterialToHU.hh"
#include <filesystem>
namespace fs = std::filesystem;
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
void ImageUtils::ImageToRawFile(InternalImageType::Pointer input, std::string savePath){
    // 1. Prepare the Raw file
    std::string rawPath = savePath + 
                    std::to_string(input->GetLargestPossibleRegion().GetSize()[0]) + "x" + 
                    std::to_string(input->GetLargestPossibleRegion().GetSize()[1]) + "x" + 
                    std::to_string(input->GetLargestPossibleRegion().GetSize()[2]) + ".raw";
    std::ofstream rawFile(rawPath, std::ios::binary);

    // 2. Iterate through the volume
    itk::ImageRegionConstIterator<InternalImageType> it(input, input->GetLargestPossibleRegion());

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        auto value = it.Get();
        rawFile.write(reinterpret_cast<const char*>(&value), sizeof(typename InternalImageType::PixelType));
    }

    rawFile.close();
}

G4DatReader::LabelImageType::Pointer ImageUtils::OrganLabelsToMaterialLabels(G4DatReader::LabelImageType::Pointer organLabels) {
    // Create output image
    auto materialLabels = G4DatReader::LabelImageType::New();
    materialLabels->SetRegions(organLabels->GetLargestPossibleRegion());
    materialLabels->SetSpacing(organLabels->GetSpacing());
    materialLabels->SetOrigin(organLabels->GetOrigin());
    materialLabels->SetDirection(organLabels->GetDirection());
    materialLabels->Allocate();

    // Iterate through the organ labels and map to material labels
    itk::ImageRegionConstIterator<G4DatReader::LabelImageType> itIn(organLabels, organLabels->GetLargestPossibleRegion());
    itk::ImageRegionIterator<G4DatReader::LabelImageType> itOut(materialLabels, materialLabels->GetLargestPossibleRegion());

    for (itIn.GoToBegin(), itOut.GoToBegin(); !itIn.IsAtEnd(); ++itIn, ++itOut) {
        int organID = static_cast<int>(itIn.Get());
        unsigned short materialID = static_cast<unsigned short>(G4DatReader::MapOrganToMaterial(organID));
        itOut.Set(materialID);
    }

    return materialLabels;
}

void ImageUtils::LabelsToRawFile(G4DatReader::LabelImageType::Pointer finalImage, const std::string& filename_base){
    auto spacing = finalImage->GetSpacing();
    auto region = finalImage->GetLargestPossibleRegion();
    auto size = region.GetSize();
    itk::Vector<double, 3> origin;
    for (int i = 0; i < 3; ++i) {
        origin[i] = -static_cast<double>(size[i]) * spacing[i]/2.0;
    }

    // 1. Prepare the Raw file (8-bit unsigned)
    std::string filename = filename_base + 
                      std::to_string(size[0]) + "x" + 
                      std::to_string(size[1]) + "x" + 
                      std::to_string(size[2]);
    std::string rawPath = filename + ".raw";
    std::ofstream rawFile(rawPath, std::ios::binary);

    // 2. Iterate through the volume
    itk::ImageRegionConstIterator<G4DatReader::LabelImageType> it(finalImage, region);

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        unsigned char materialID = static_cast<unsigned char>(it.Get());
        rawFile.write(reinterpret_cast<char*>(&materialID), sizeof(unsigned char));
    }
    rawFile.close();

    // 2. Open the info file
    std::ofstream infoFile(filename + ".txt");

    fs::path tempPath = filename;
    auto filename_plain = tempPath.filename().string();

    // Set precision for floats to ensure the decimal points look right
    infoFile << std::fixed << std::setprecision(3);

    infoFile << "#[SECTION VOXELIZED GEOMETRY FILE v.2017-07-26]\n";
    infoFile << "phantom/"<<filename_plain << ".raw     # VOXEL GEOMETRY FILE (penEasy 2008 format; .gz accepted)\n";

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
    infoFile << " 0 0 0                          # SIZE OF LOW RESOLUTION VOXELS\n";

    infoFile.close();

    std::cout << "MCGPU info file generated: " << filename << ".txt" << std::endl;
}

G4DatReader::LabelImageType::Pointer ImageUtils::GetSegmentedLabelsFromFullPhantom(
    G4DatReader::LabelImageType::Pointer fullPhantom,
    std::string maskPath){

    // 2. Load the VOI mask you exported from Slicer
    auto maskReader = itk::ImageFileReader<G4DatReader::LabelImageType>::New();
    maskReader->SetFileName(maskPath);
    maskReader->Update();

    using MaskFilterType = itk::MaskImageFilter<G4DatReader::LabelImageType, G4DatReader::LabelImageType>;
    auto maskFilter = MaskFilterType::New();

    maskFilter->SetInput(fullPhantom);      // Input 1: The whole ICRP volume
    maskFilter->SetMaskImage(maskReader->GetOutput()); // Input 2: Your Slicer VOI
    maskFilter->SetOutsideValue(0);         // Pixels outside the mask become Air (0)
    // 3. Find the region and Crop
    auto voiRegion = ImageUtils::GetRegionFromMask(maskReader->GetOutput());

    auto roiFilter = itk::RegionOfInterestImageFilter<G4DatReader::LabelImageType, G4DatReader::LabelImageType>::New();
    roiFilter->SetInput(maskFilter->GetOutput());
    roiFilter->SetRegionOfInterest(voiRegion);
    roiFilter->Update();

    //resetto l'origine al centro del volumi
    auto vol = roiFilter->GetOutput();
    double origin[3];
    origin[0] = static_cast<double>(voiRegion.GetSize()[0]) * vol->GetSpacing()[0]/2.0;
    origin[1] = static_cast<double>(voiRegion.GetSize()[1]) * vol->GetSpacing()[1]/2.0;
    origin[2] = static_cast<double>(voiRegion.GetSize()[2]) * vol->GetSpacing()[2]/2.0;
    origin[0] = 0;
    origin[1] = 0;
    origin[2] = 0;
    vol->SetOrigin(origin);
    return vol;
}

ImageUtils::InternalImageType::Pointer ImageUtils::CreateHUPhantom(G4DatReader::LabelImageType::Pointer organImage, G4double energy, G4DatReader::PhantomSex sex)
{
    auto huImage = ImageUtils::InternalImageType::New();
    huImage->SetRegions(organImage->GetLargestPossibleRegion());
    huImage->SetSpacing(organImage->GetSpacing());
    huImage->SetOrigin(organImage->GetOrigin());
    huImage->SetDirection(organImage->GetDirection());
    huImage->Allocate();

    itk::ImageRegionConstIterator<G4DatReader::LabelImageType> itIn(organImage, organImage->GetLargestPossibleRegion());
    itk::ImageRegionIterator<ImageUtils::InternalImageType> itOut(huImage, huImage->GetLargestPossibleRegion());
    G4MaterialToHU* materialToHU = new G4MaterialToHU(energy, sex);

    for (itIn.GoToBegin(), itOut.GoToBegin(); !itIn.IsAtEnd(); ++itIn, ++itOut) {
        itOut.Set(materialToHU->GetHUForMaterial(G4DatReader::MapOrganToMaterial(itIn.Get())));
    }
    delete materialToHU;
    return huImage;
}
G4DatReader::LabelImageType::Pointer ImageUtils::InsertPhysicalVerticalBarella(
    G4DatReader::LabelImageType::Pointer labels, 
    double yStartFromCenter_mm, 
    double carbonThick_mm, 
    double foamThick_mm) 
{
    using LabelImageType = G4DatReader::LabelImageType;


    
    // 1. Define Physical Boundaries (Sandwich layers in mm)
    double carbon1End = yStartFromCenter_mm + carbonThick_mm;
    double foamEnd    = carbon1End + foamThick_mm;
    double carbon2End = foamEnd;

    const int ID_CARBON = 53;
    const int ID_FOAM = 54;

    double spacingY = labels->GetSpacing()[1];
    double sizeY_mm = labels->GetLargestPossibleRegion().GetSize()[1] * spacingY;


     // 3.5 Determine the "Maximum" size needed (e.g., 512x512x512 or max of both)
    itk::Size<3> targetSize;
    targetSize[0] = labels->GetLargestPossibleRegion().GetSize()[0];
    targetSize[1] = std::max(labels->GetLargestPossibleRegion().GetSize()[1]*spacingY, 
                             2*carbon2End)/spacingY;
    targetSize[2] = labels->GetLargestPossibleRegion().GetSize()[2];

    // 4. Pad everything to the same centered grid
    // For DICOM, pad with -1024 (Air HU) instead of 0
    auto labelsPadded = ImageUtils::CenterPadToSize<G4DatReader::LabelImageType>(labels, targetSize, 0);

    // 2. Iterate through the image
    itk::ImageRegionIteratorWithIndex<LabelImageType> it(
        labelsPadded, labelsPadded->GetLargestPossibleRegion());

    double centerY_mm = targetSize[1]*spacingY/2.0;

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        // Transform the current voxel index to a physical point (x, y, z in mm)
        LabelImageType::PointType physicalPoint;
        labelsPadded->TransformIndexToPhysicalPoint(it.GetIndex(), physicalPoint);
        //it.Set(0); // Default to Air
        double y_mm = physicalPoint[1]-centerY_mm; // Get Y coordinate in mm

        // 3. Assign Materials based on mm boundaries
        if (y_mm >= yStartFromCenter_mm && y_mm < carbon1End) {
            it.Set(ID_CARBON); // Carbon
        }
        else if (y_mm >= carbon1End && y_mm < foamEnd) {
            it.Set(ID_FOAM); // Foam
        }
        else if (y_mm >= foamEnd && y_mm < carbon2End) {
            it.Set(ID_CARBON); // Carbon
        }
    }
    return labelsPadded;
}