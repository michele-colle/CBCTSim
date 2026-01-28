#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkResampleImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkNearestNeighborInterpolateImageFunction.h>
#include <itkDirectory.h>
#include <itksys/SystemTools.hxx>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

// Function to perform natural sorting (slice1, slice2, slice10...)
bool naturalSort(const std::string& a, const std::string& b) {
    auto extractNum = [](const std::string& s) {
        std::string num;
        for (char c : s) if (std::isdigit(c)) num += c;
        return num.empty() ? 0 : std::stoi(num);
    };
    return extractNum(a) < extractNum(b);
}
int MapOrganToMaterial(int OrgID) {
    if (OrgID == 128) return 1; // teeth
    // Bone group
    if (OrgID == 13 || OrgID == 16 || OrgID == 19 || OrgID == 22 || OrgID == 24 || 
        OrgID == 26 || OrgID == 28 || OrgID == 31 || OrgID == 34 || OrgID == 37 || 
        OrgID == 39 || OrgID == 41 || OrgID == 43 || OrgID == 45 || OrgID == 47 || 
        OrgID == 49 || OrgID == 51 || OrgID == 53 || OrgID == 55) return 2;
    
    // ... continue mapping for IDs 3 through 52 ...
    // Note: Use the mapping from ReadPhantomDataFile in your .cc file

    if (OrgID == 0 || OrgID == 140) return 0; // Air
    
    // Default: if no mapping exists, return OrgID or a default
    return OrgID; 
}
float MapToHU(int materialID) {
    switch (materialID) {
        case 0:  return -1000.0f; // Air (Black)
        
        // --- BONES & DENSE STRUCTURES ---
        case 1:  return 1500.0f;  // Teeth (Very Dense)
        case 8:  return 1000.0f;  // Cranium
        case 2:  
        case 9:  
        case 10: 
        case 14: 
        case 17: 
        case 18: 
        case 19: return 800.0f;   // Major Cortical Bones (Femur, Pelvis, Spine)
        
        case 3:  
        case 4:  
        case 7:  
        case 13: 
        case 15: 
        case 16: 
        case 20: 
        case 21: return 600.0f;   // Other Bones (Ribs, Sternum, Mandible)

        case 5:  
        case 6:  
        case 11: 
        case 12: return 400.0f;   // Distal bones (Hand, Foot, Lower arms/legs)
        
        // --- SOFT TISSUES (Standard ~40 HU) ---
        case 28: return 60.0f;    // Blood (Slightly denser)
        case 29: return 50.0f;    // Muscle
        case 30: return 60.0f;    // Liver
        case 33: return 40.0f;    // Heart
        case 35: return 30.0f;    // Kidney
        case 39: return 45.0f;    // Spleen
        
        // --- ADIPOSE / GLANDULAR ---
        case 49: return -100.0f;  // Breast Adipose (Fat is negative HU)
        case 48: return 20.0f;    // Breast Glandular
        
        // --- ORGANS WITH AIR/FLUID MIX ---
        case 50: return -700.0f;  // Lung (Highly non-dense, crucial for registration)
        case 51: return -150.0f;  // Gastro Content (Mix of air/food)
        case 52: return 0.0f;     // Urine (Water density)
        case 41: return 10.0f;    // Bladder (Fluid filled)

        // --- GENERAL SOFT TISSUE (Default) ---
        default: 
            if (materialID > 0 && materialID <= 52) return 40.0f; 
            std::cout << "Warning: Unknown material ID " << materialID << ", defaulting to -1000 HU (Air)." << std::endl;
            return -1000.0f; // Default to Air if unknown
    }
}

int main() {
    std::string inputFolder = "/home/colle/CBCTSim/build/cpu-release/ICRPdata/ICRP110_g4dat/AF/";
    
    // 1. Find and Sort Files
    auto itkDir = itk::Directory::New();
    itkDir->Load(inputFolder.c_str());
    std::vector<std::string> files;
    for(size_t i=0; i<itkDir->GetNumberOfFiles(); ++i) {
        std::string f = itkDir->GetFile(i);
        if(f.find(".g4dat") != std::string::npos) files.push_back(f);
    }
    std::sort(files.begin(), files.end(), naturalSort);

    // 2. Read first slice to get dimensions
    std::ifstream firstFile(inputFolder + files[0]);
    int nx, ny;
    firstFile >> nx >> ny;
    int nz = files.size();

    // 3. Create ITK Images
    using HUImageType = itk::Image<float, 3>; // For Synthetic CT
    using LabelImageType = itk::Image<unsigned short, 3>; // For Raw IDs
    
    auto huImage = HUImageType::New();
    auto labelImage = LabelImageType::New();

    HUImageType::RegionType region;
    HUImageType::SizeType size = {{ (size_t)nx, (size_t)ny, (size_t)nz }};
    region.SetSize(size);
    huImage->SetRegions(region);
    huImage->Allocate();
    labelImage->SetRegions(region);
    labelImage->Allocate();

    // ICRP 110 AF spacing is typically 1.775 x 1.775 x 4.84 mm
    // Verify these values for your specific phantom!
    double spacing[3] = {0.8875, 0.8875, 2.42};
    huImage->SetSpacing(spacing);
    labelImage->SetSpacing(spacing);

    // 4. Load Data
    std::cout << "Loading " << nz << " slices..." << std::endl;
    for (int z = 0; z < nz; ++z) {
        std::ifstream sliceFile(inputFolder + files[z]);
        int tx, ty, junk;
        sliceFile >> tx >> ty >> junk; // Skip header lines
        
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) {
                int val;
                sliceFile >> val;
                HUImageType::IndexType idx = {{x, y, z}};
                huImage->SetPixel(idx, MapToHU(val));
                labelImage->SetPixel(idx, val);
            }
        }
    }

    // 5. Scaling (Resampling)
    // Let's resample to 1mm isotropic for better registration performance
    using ResampleFilter = itk::ResampleImageFilter<HUImageType, HUImageType>;
    auto resampler = ResampleFilter::New();
    resampler->SetInput(huImage);
    
    double outSpacing[3] = {0.8875, 0.8875, 2.42};
    resampler->SetOutputSpacing(outSpacing);
    resampler->SetOutputOrigin(huImage->GetOrigin());
    resampler->SetOutputDirection(huImage->GetDirection());

    HUImageType::SizeType outSize;
    for(int i=0; i<3; ++i) 
        outSize[i] = (size_t)(size[i] * spacing[i] / outSpacing[i]);
    std::cout<< "Resampling to size: " 
              << outSize[0] << " x " 
              << outSize[1] << " x " 
              << outSize[2] << std::endl;
    
    resampler->SetSize(outSize);
    resampler->SetInterpolator(itk::LinearInterpolateImageFunction<HUImageType>::New());
    resampler->Update();

    // 6. Save results
    auto writer = itk::ImageFileWriter<HUImageType>::New();
    writer->SetFileName("icrp_synthetic_hu.nrrd");
    writer->SetInput(resampler->GetOutput());
    writer->Update();

    auto lWriter = itk::ImageFileWriter<LabelImageType>::New();
    lWriter->SetFileName("icrp_labels.nrrd");
    lWriter->SetInput(labelImage);
    lWriter->Update();

    std::cout << "Done! Created synthetic CT and label volume." << std::endl;
    return 0;
}