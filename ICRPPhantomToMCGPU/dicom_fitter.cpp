#include "ImageUtils.hpp"
#include "G4DatReader.hpp"
#include <itkImageSeriesReader.h>
#include <itkImageFileWriter.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkConstantPadImageFilter.h>
#include <itkDirectory.h>
#include <itksys/SystemTools.hxx> // For path unwinding
#include "G4MaterialToHU.hh"
#include "G4SystemOfUnits.hh"

ImageUtils::InternalImageType::Pointer ReadDicom(std::string dirName)
{
    std::string absolutePath = itksys::SystemTools::CollapseFullPath(dirName);
    
    auto nameGenerator = itk::GDCMSeriesFileNames::New();
    nameGenerator->SetDirectory(absolutePath);

    try {
        const auto & seriesUIDs = nameGenerator->GetSeriesUIDs();
        if (seriesUIDs.empty()) {
            throw std::runtime_error("No DICOM series found in " + absolutePath);
        }

        std::vector<std::string> fileNames = nameGenerator->GetFileNames(seriesUIDs[0]);

        auto reader = itk::ImageSeriesReader<ImageUtils::InternalImageType>::New();
        reader->SetImageIO(itk::GDCMImageIO::New());
        reader->SetFileNames(fileNames);
        reader->Update();

        //resetto l'origine al centro del volumi
        auto vol = reader->GetOutput();
        double origin[3];
        origin[0] = static_cast<double>(vol->GetLargestPossibleRegion().GetSize()[0]) * vol->GetSpacing()[0]/2.0;
        origin[1] = static_cast<double>(vol->GetLargestPossibleRegion().GetSize()[1]) * vol->GetSpacing()[1]/2.0;
        origin[2] = static_cast<double>(vol->GetLargestPossibleRegion().GetSize()[2]) * vol->GetSpacing()[2]/2.0;
        origin[0] = 0;
        origin[1] = 0;
        origin[2] = 0;
        vol->SetOrigin(origin);
        return vol;
    } catch (itk::ExceptionObject &ex) {
        std::cerr << "ITK Exception: " << ex << std::endl;
        return nullptr;
    }
}

void CreateUniformVolumesForFitting()
{
    auto dicomVolume = ReadDicom("data/dicom_data_folder");
    auto organPhantom = ImageUtils::GetSegmentedLabelsFromFullPhantom(
        G4DatReader::LoadPhantom("./data/ICRPdata", G4DatReader::PhantomSex::Female),
        "./data/ICRP_segmentation_data/Segmentation-Female-Head.nrrd"
    );
    // 2. Convert Organ IDs to HU
    auto huPhantom = ImageUtils::CreateHUPhantom(organPhantom, 60.0*keV, G4DatReader::PhantomSex::Female);
    // 3. Resample both to same square resolution (e.g., 1.5mm)
    double targetRes = 0.5; 
    auto finalDicom = ImageUtils::ResampleImage<ImageUtils::InternalImageType>(dicomVolume, targetRes);
    auto finalHUPhantom = ImageUtils::ResampleImage<ImageUtils::InternalImageType>(huPhantom, targetRes);
    
    // Note: Resample the Label Phantom using Nearest Neighbor to keep IDs integer
    auto finalLabels = ImageUtils::ResampleImage<G4DatReader::LabelImageType>(organPhantom, targetRes);


    // 3.5 Determine the "Maximum" size needed (e.g., 512x512x512 or max of both)
    itk::Size<3> targetSize;
    targetSize[0] = std::max(finalDicom->GetLargestPossibleRegion().GetSize()[0], 
                             finalHUPhantom->GetLargestPossibleRegion().GetSize()[0]);
    targetSize[1] = std::max(finalDicom->GetLargestPossibleRegion().GetSize()[1], 
                             finalHUPhantom->GetLargestPossibleRegion().GetSize()[1]);
    targetSize[2] = std::max(finalDicom->GetLargestPossibleRegion().GetSize()[2], 
                             finalHUPhantom->GetLargestPossibleRegion().GetSize()[2]);

    // 4. Pad everything to the same centered grid
    // For DICOM, pad with -1024 (Air HU) instead of 0
    auto centeredDicom = ImageUtils::CenterPadToSize<ImageUtils::InternalImageType>(finalDicom, targetSize, -1024.0);
    auto centeredHU = ImageUtils::CenterPadToSize<ImageUtils::InternalImageType>(finalHUPhantom, targetSize, -1024.0);
    auto centeredLabels = ImageUtils::CenterPadToSize<G4DatReader::LabelImageType>(finalLabels, targetSize, 0);

    // 4. Save for Python
    auto writer = itk::ImageFileWriter<ImageUtils::InternalImageType>::New();
    writer->SetFileName("./output/python_dicom.nrrd");
    writer->SetInput(centeredDicom);
    writer->UseCompressionOff();
    writer->Update();
    writer->SetFileName("./output/python_hu_phantom.nrrd");
    writer->SetInput(centeredHU);
    writer->UseCompressionOff();
    writer->Update();

    ImageUtils::ImageToRawFile(centeredHU, "./output/python_hu_phantom");
    ImageUtils::ImageToRawFile(centeredDicom, "./output/python_dicom");


    auto labelWriter = itk::ImageFileWriter<G4DatReader::LabelImageType>::New();
    labelWriter->SetFileName("./output/python_labels.nrrd");
    labelWriter->SetInput(centeredLabels);
    labelWriter->UseCompressionOff();
    labelWriter->Update();
}
void nrrdLabelsToRawLabelsPlusBarella(std::string nrrdPath, std::string rawPath)
{
    auto labelReader = itk::ImageFileReader<G4DatReader::LabelImageType>::New();
    labelReader->SetFileName(nrrdPath);
    labelReader->Update();
    auto materialLabels = ImageUtils::OrganLabelsToMaterialLabels(labelReader->GetOutput());
    auto materialLabelsBarella = ImageUtils::InsertPhysicalVerticalBarella(materialLabels, 140, 5, 36);
    ImageUtils::LabelsToRawFile(materialLabelsBarella, rawPath);
}
int main()
{
    //CreateUniformVolumesForFitting();
    nrrdLabelsToRawLabelsPlusBarella("./output/registered_labels.nrrd", "./output/female_head_phantom_registered");

    return 0;
}