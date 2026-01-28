#include "itkImage.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include "itkImageSeriesReader.h"
#include "itkImageFileWriter.h"
#include "itkDirectory.h"
#include "itksys/SystemTools.hxx" // For path unwinding

int main(int argc, char* argv[]) {
    // 1. Unwind and Debug Path
    std::string dirName = "../dicom_data_folder/"; 
    std::string absolutePath = itksys::SystemTools::CollapseFullPath(dirName);
    std::string cwd = itksys::SystemTools::GetCurrentWorkingDirectory();

    std::cout << "--- Environment Debug ---" << std::endl;
    std::cout << "Current Working Dir: " << cwd << std::endl;
    std::cout << "Target Relative Path: " << dirName << std::endl;
    std::cout << "Target Absolute Path: " << absolutePath << std::endl;
    std::cout << "-------------------------\n" << std::endl;

    // 2. Directory Listing
    auto itkDir = itk::Directory::New();
    if (!itkDir->Load(absolutePath.c_str())) {
        std::cerr << "FATAL ERROR: Directory does not exist at " << absolutePath << std::endl;
        return EXIT_FAILURE;
    }

    size_t fileCount = itkDir->GetNumberOfFiles();
    std::cout << "Found " << fileCount << " entries. First 5:" << std::endl;
    for (size_t i = 0; i < std::min(fileCount, size_t(5)); i++) {
        std::cout << "  - " << itkDir->GetFile(i) << std::endl;
    }

    // 3. ITK DICOM Logic
    auto nameGenerator = itk::GDCMSeriesFileNames::New();
    nameGenerator->SetDirectory(absolutePath);

    try {
        const auto & seriesUIDs = nameGenerator->GetSeriesUIDs();
        if (seriesUIDs.empty()) {
            std::cerr << "No DICOM series found in " << absolutePath << std::endl;
            return EXIT_FAILURE;
        }

        std::string seriesIdentifier = seriesUIDs[0];
        std::vector<std::string> fileNames = nameGenerator->GetFileNames(seriesIdentifier);

        auto reader = itk::ImageSeriesReader<itk::Image<signed short, 3>>::New();
        reader->SetImageIO(itk::GDCMImageIO::New());
        reader->SetFileNames(fileNames);
        
        std::cout << "\nReading " << fileNames.size() << " slices..." << std::endl;
        
        auto writer = itk::ImageFileWriter<itk::Image<signed short, 3>>::New();
        writer->SetFileName("output_volume.nrrd");
        writer->SetInput(reader->GetOutput());
        writer->Update();
        
        std::cout << "SUCCESS: Saved output_volume.nrrd" << std::endl;

    } catch (itk::ExceptionObject &ex) {
        std::cerr << "ITK Exception: " << ex << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}