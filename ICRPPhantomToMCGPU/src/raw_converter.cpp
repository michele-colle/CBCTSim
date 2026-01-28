#include <itkImageFileWriter.h>
#include <itkImageFileReader.h>
#include <itkRegionOfInterestImageFilter.h>
#include <itkMaskImageFilter.h>
#include <itkImage.h>
#include "G4DatReader.hpp"
#include "ImageUtils.hpp"
void G4DataToNRRDFullPhantoms(){
    auto fem = G4DatReader::LoadPhantom("./data/ICRPdata", G4DatReader::PhantomSex::Female);
    auto lWriter = itk::ImageFileWriter<G4DatReader::LabelImageType>::New();
    lWriter->SetFileName("./output/icrp_female_organs_labels.nrrd");
    lWriter->SetInput(fem);
    lWriter->Update();
    auto male = G4DatReader::LoadPhantom("./data/ICRPdata", G4DatReader::PhantomSex::Male);
    lWriter->SetFileName("./output/icrp_male_organs_labels.nrrd");
    lWriter->SetInput(male);
    lWriter->Update();
}

int main()
{
    //questo serve per creare i phantom da visualizzare con 3D slicer
    //G4DataToNRRDFullPhantoms();


    // 1. Load the full phantom (Organs)
    auto fullPhantom = G4DatReader::LoadPhantom("./data/ICRPdata", G4DatReader::PhantomSex::Female);

    // 2. Load the VOI mask you exported from Slicer
    auto maskReader = itk::ImageFileReader<G4DatReader::LabelImageType>::New();
    maskReader->SetFileName("./data/ICRP_segmentation_data/female_head_test.nrrd");
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

    auto lWriter = itk::ImageFileWriter<G4DatReader::LabelImageType>::New();
    lWriter->SetFileName("./output/icrp_female_organs_labels_segmented.nrrd");
    lWriter->SetInput(roiFilter->GetOutput());
    lWriter->Update();

    ImageUtils::OrganLabelToMaterialLabelRawFile(roiFilter->GetOutput(), "./output/icrp_female_head_material");

    return 0;
}