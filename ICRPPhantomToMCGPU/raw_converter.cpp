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

void ExtractVOIFromFullPhantom(){
    // 1. Load the full phantom (Organs)
    auto fullPhantom = G4DatReader::LoadPhantom("./data/ICRPdata", G4DatReader::PhantomSex::Female);
    auto segmentedLabels = ImageUtils::GetSegmentedLabelsFromFullPhantom(
        fullPhantom,
        "./data/ICRP_segmentation_data/Segmentation-Female-Head.nrrd"
    );

    auto lWriter = itk::ImageFileWriter<G4DatReader::LabelImageType>::New();
    lWriter->SetFileName("./output/icrp_female_organs_labels_segmented.nrrd");
    lWriter->SetInput(segmentedLabels);
    lWriter->Update();

    ImageUtils::OrganLabelToMaterialLabelRawFile(segmentedLabels, "./output/icrp_female_head_material");
}

int main()
{
    //questo serve per creare i phantom da visualizzare con 3D slicer
    //G4DataToNRRDFullPhantoms();

    ExtractVOIFromFullPhantom();


    

    return 0;
}