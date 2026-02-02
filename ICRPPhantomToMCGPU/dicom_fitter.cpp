#include "ImageUtils.hpp"
#include "G4DatReader.hpp"
#include <itkImageSeriesReader.h>
#include <itkImageFileWriter.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkDirectory.h>
#include <itksys/SystemTools.hxx> // For path unwinding
#include "G4MaterialToHU.hh"
#include "G4SystemOfUnits.hh"

using InternalImageType = itk::Image<float, 3>;
InternalImageType::Pointer ReadDicom(std::string dirName)
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

        auto reader = itk::ImageSeriesReader<InternalImageType>::New();
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
        
        return reader->GetOutput();

    } catch (itk::ExceptionObject &ex) {
        std::cerr << "ITK Exception: " << ex << std::endl;
        return nullptr;
    }
}

// Helper to convert Organ ID to Hounsfield Units
InternalImageType::Pointer CreateHUPhantom(G4DatReader::LabelImageType::Pointer organImage, G4double energy, G4DatReader::PhantomSex sex) {
    auto huImage = InternalImageType::New();
    huImage->SetRegions(organImage->GetLargestPossibleRegion());
    huImage->SetSpacing(organImage->GetSpacing());
    huImage->SetOrigin(organImage->GetOrigin());
    huImage->SetDirection(organImage->GetDirection());
    huImage->Allocate();

    itk::ImageRegionConstIterator<G4DatReader::LabelImageType> itIn(organImage, organImage->GetLargestPossibleRegion());
    itk::ImageRegionIterator<InternalImageType> itOut(huImage, huImage->GetLargestPossibleRegion());
    G4MaterialToHU materialToHU(energy, sex);

    for (itIn.GoToBegin(), itOut.GoToBegin(); !itIn.IsAtEnd(); ++itIn, ++itOut) {
        itOut.Set(materialToHU.GetHUForMaterial(G4DatReader::MapOrganToMaterial(itIn.Get())));
    }
    return huImage;
}

int main()
{
    auto dicomVolume = ReadDicom("data/dicom_data_folder");
    auto organPhantom = ImageUtils::GetSegmentedLabelsFromFullPhantom(
        G4DatReader::LoadPhantom("./data/ICRPdata", G4DatReader::PhantomSex::Female),
        "./data/ICRP_segmentation_data/Segmentation-Female-Head.nrrd"
    );
    // 2. Convert Organ IDs to HU
    auto huPhantom = CreateHUPhantom(organPhantom, 60.0*keV, G4DatReader::PhantomSex::Female);
    // 3. Resample both to same square resolution (e.g., 1.5mm)
    double targetRes = 0.5; 
    auto finalDicom = ImageUtils::ResampleImage<InternalImageType>(dicomVolume, targetRes);
    auto finalHUPhantom = ImageUtils::ResampleImage<InternalImageType>(huPhantom, targetRes);
    
    // Note: Resample the Label Phantom using Nearest Neighbor to keep IDs integer
    auto finalLabels = ImageUtils::ResampleImage<G4DatReader::LabelImageType>(organPhantom, targetRes);

    // 4. Save for Python
    auto writer = itk::ImageFileWriter<InternalImageType>::New();
    writer->SetFileName("./output/python_dicom.nrrd");
    writer->SetInput(finalDicom);
    writer->UseCompressionOff();
    writer->Update();
    writer->SetFileName("./output/python_hu_phantom.nrrd");
    writer->SetInput(finalHUPhantom);
    writer->UseCompressionOff();
    writer->Update();

    auto labelWriter = itk::ImageFileWriter<G4DatReader::LabelImageType>::New();
    labelWriter->SetFileName("./output/python_labels.nrrd");
    labelWriter->SetInput(finalLabels);
    labelWriter->UseCompressionOff();
    labelWriter->Update();

    return 0;
}