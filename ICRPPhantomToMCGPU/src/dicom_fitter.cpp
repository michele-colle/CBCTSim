#include "ImageUtils.hpp"
#include "G4DatReader.hpp"
#include <itkImageSeriesReader.h>
#include <itkImageFileWriter.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkDirectory.h>
#include <itksys/SystemTools.hxx> // For path unwinding


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
        
        return reader->GetOutput();

    } catch (itk::ExceptionObject &ex) {
        std::cerr << "ITK Exception: " << ex << std::endl;
        return nullptr;
    }
}

// Helper to convert Organ ID to Hounsfield Units
InternalImageType::Pointer CreateHUPhantom(G4DatReader::LabelImageType::Pointer organImage) {
    auto huImage = InternalImageType::New();
    huImage->SetRegions(organImage->GetLargestPossibleRegion());
    huImage->SetSpacing(organImage->GetSpacing());
    huImage->SetOrigin(organImage->GetOrigin());
    huImage->SetDirection(organImage->GetDirection());
    huImage->Allocate();

    itk::ImageRegionConstIterator<G4DatReader::LabelImageType> itIn(organImage, organImage->GetLargestPossibleRegion());
    itk::ImageRegionIterator<InternalImageType> itOut(huImage, huImage->GetLargestPossibleRegion());

    for (itIn.GoToBegin(), itOut.GoToBegin(); !itIn.IsAtEnd(); ++itIn, ++itOut) {
        itOut.Set(G4DatReader::MapMaterialToHU(G4DatReader::MapOrganToMaterial(itIn.Get())));
    }
    return huImage;
}

int main()
{
    auto dicomVolume = ReadDicom("dicom_folder_path");
    // Assume organPhantom is the NRRD you saved earlier
    auto reader = itk::ImageFileReader<G4DatReader::LabelImageType>::New();
    reader->SetFileName("data/ICRP_segmentation_data/female_head_test.nrrd");
    reader->Update();
    auto organPhantom = reader->GetOutput();
    // 2. Convert Organ IDs to HU
    auto huPhantom = CreateHUPhantom(organPhantom);
    // 3. Resample both to same square resolution (e.g., 1.5mm)
    double targetRes = 1.5; 
    auto finalDicom = ImageUtils::ResampleImage<InternalImageType>(dicomVolume, targetRes);
    auto finalHUPhantom = ImageUtils::ResampleImage<InternalImageType>(huPhantom, targetRes);
    
    // Note: Resample the Label Phantom using Nearest Neighbor to keep IDs integer
    auto finalLabels = ImageUtils::ResampleImage<G4DatReader::LabelImageType>(organPhantom, targetRes);

    // 4. Save for Python
    auto writer = itk::ImageFileWriter<InternalImageType>::New();
    writer->SetFileName("python_dicom.nrrd");
    writer->SetInput(finalDicom);
    writer->UseCompressionOff();
    writer->Update();
    writer->SetFileName("python_hu_phantom.nrrd");
    writer->SetInput(finalHUPhantom);
    writer->UseCompressionOff();
    writer->Update();

    auto labelWriter = itk::ImageFileWriter<G4DatReader::LabelImageType>::New();
    labelWriter->SetFileName("python_labels.nrrd");
    labelWriter->SetInput(finalLabels);
    labelWriter->UseCompressionOff();
    labelWriter->Update();

    return 0;
}