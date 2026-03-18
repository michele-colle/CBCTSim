// dicom_to_mcgpu.cpp
// Reads a DICOM CT series, applies HU thresholding and writes an MC-GPU ready
// uint8 label volume + companion geometry .txt file.
//
// Usage (cfg file):
//   dicom_to_mcgpu  my_scan.cfg
//
// Usage (positional):
//   dicom_to_mcgpu  <dicom_dir> [output_prefix]
//
// Label scheme (same as mcrp_to_vox):
//   0 = air          (HU < thr_air_fat)
//   1 = fat          (thr_air_fat  <= HU < thr_fat_soft)
//   2 = soft tissue  (thr_fat_soft <= HU < thr_soft_spongiosa)
//   3 = bone spongiosa (thr_soft_spongiosa <= HU < thr_spongiosa_cort)
//   4 = bone cortical  (HU >= thr_spongiosa_cort)
//   5 = implant      (cylinder override, optional)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageSeriesReader.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Config-file parser: reads "key = value" lines, ignores # comments
// (identical to the one in mcrp_to_vox.cpp)
// ---------------------------------------------------------------------------
static std::map<std::string, std::string> loadCfg(const std::string &path)
{
    std::map<std::string, std::string> cfg;
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open config file: " + path);
    std::string line;
    while (std::getline(f, line))
    {
        auto ch = line.find('#');
        if (ch != std::string::npos) line.erase(ch);
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto trim = [](std::string s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
            return s;
        };
        cfg[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return cfg;
}

// ---------------------------------------------------------------------------
// Implant cylinder (axis along Z)
// ---------------------------------------------------------------------------
struct ImplantCylinder {
    double cx_mm, cy_mm, cz_mm, radius_mm, height_mm;
};

// ---------------------------------------------------------------------------
// Write MC-GPU [SECTION VOXELIZED GEOMETRY FILE] companion .txt
// (same format produced by mcrp_to_vox)
// ---------------------------------------------------------------------------
static void writeInfoFile(const std::string &base,
                          size_t nx, size_t ny, size_t nz,
                          double sx_cm, double sy_cm, double sz_cm)
{
    const std::string txtPath = base + ".txt";
    std::ofstream f(txtPath);
    if (!f) throw std::runtime_error("Cannot write info file: " + txtPath);

    const std::string fname = fs::path(base).filename().string();

    f << std::fixed << std::setprecision(3);
    f << "#[SECTION VOXELIZED GEOMETRY FILE v.2017-07-26]\n";
    f << "phantom/" << fname << ".raw"
      << "     # VOXEL GEOMETRY FILE (penEasy 2008 format; .gz accepted)\n";
    // centred offset
    f << " " << -(static_cast<double>(nx) * sx_cm / 2.0)
      << "  " << -(static_cast<double>(ny) * sy_cm / 2.0)
      << "  " << -(static_cast<double>(nz) * sz_cm / 2.0)
      << "              # OFFSET OF THE VOXEL GEOMETRY [cm]\n";
    f << " " << nx << " " << ny << " " << nz
      << "                 # NUMBER OF VOXELS\n";
    f << " " << sx_cm << " " << sy_cm << " " << sz_cm
      << "           # VOXEL SIZES [cm]\n";
    f << " 0 0 0                          # SIZE OF LOW RESOLUTION VOXELS\n";

    std::cout << "Info file written: " << txtPath << "\n";
}

// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    // -------------------------------------------------------------------------
    // 1. Parse parameters
    // -------------------------------------------------------------------------
    std::string dicomDir     = ".";
    std::string outputPrefix = "dicom";
    // HU thresholds (same defaults as mcrp_to_vox)
    int16_t thr_air_fat        = -500;
    int16_t thr_fat_soft       =  -50;
    int16_t thr_soft_spongiosa =  200;
    int16_t thr_spongiosa_cort =  800;
    // optional series UID (leave empty → pick first found)
    std::string seriesUID;
    // crop cylinder
    bool   crop_cylinder_enable    = false;
    double crop_cylinder_radius_mm = 100.0;
    double crop_cylinder_cx_mm     = 0.0;
    double crop_cylinder_cy_mm     = 0.0;
    // implant cylinders
    std::vector<ImplantCylinder> implants;

    const bool usingCfg = (argc == 2) &&
        (std::string(argv[1]).size() > 4) &&
        (std::string(argv[1]).substr(std::string(argv[1]).size() - 4) == ".cfg");

    if (usingCfg)
    {
        auto cfg = loadCfg(argv[1]);
        auto getD = [&](const std::string &k, double def) {
            return cfg.count(k) ? std::stod(cfg[k]) : def;
        };
        auto getS = [&](const std::string &k, const std::string &def) {
            return cfg.count(k) ? cfg[k] : def;
        };
        dicomDir     = getS("dicom_dir",     dicomDir);
        outputPrefix = getS("output_prefix", outputPrefix);
        seriesUID    = getS("series_uid",    seriesUID);

        thr_air_fat        = static_cast<int16_t>(getD("thr_air_fat",        thr_air_fat));
        thr_fat_soft       = static_cast<int16_t>(getD("thr_fat_soft",       thr_fat_soft));
        thr_soft_spongiosa = static_cast<int16_t>(getD("thr_soft_spongiosa", thr_soft_spongiosa));
        thr_spongiosa_cort = static_cast<int16_t>(getD("thr_spongiosa_cort", thr_spongiosa_cort));

        crop_cylinder_enable    = cfg.count("crop_cylinder_enable")
            ? (cfg["crop_cylinder_enable"] == "1" || cfg["crop_cylinder_enable"] == "true")
            : crop_cylinder_enable;
        crop_cylinder_radius_mm = getD("crop_cylinder_radius_mm", crop_cylinder_radius_mm);
        crop_cylinder_cx_mm     = getD("crop_cylinder_cx_mm",     crop_cylinder_cx_mm);
        crop_cylinder_cy_mm     = getD("crop_cylinder_cy_mm",     crop_cylinder_cy_mm);

        if (cfg.count("implant_count"))
        {
            const int implant_count = std::stoi(cfg["implant_count"]);
            for (int n = 0; n < implant_count; ++n)
            {
                const std::string p = "implant_" + std::to_string(n) + "_";
                ImplantCylinder imp;
                imp.cx_mm     = getD(p + "cx_mm",     0.0);
                imp.cy_mm     = getD(p + "cy_mm",     0.0);
                imp.cz_mm     = getD(p + "cz_mm",     0.0);
                imp.radius_mm = getD(p + "radius_mm", 0.0);
                imp.height_mm = getD(p + "height_mm", 0.0);
                implants.push_back(imp);
            }
        }
        else
        {
            const double r = getD("implant_radius_mm", -1.0);
            if (r > 0.0)
            {
                ImplantCylinder imp;
                imp.cx_mm     = getD("implant_cx_mm",    0.0);
                imp.cy_mm     = getD("implant_cy_mm",    0.0);
                imp.cz_mm     = getD("implant_cz_mm",    0.0);
                imp.radius_mm = r;
                imp.height_mm = getD("implant_height_mm", 0.0);
                implants.push_back(imp);
            }
        }
        std::cout << "Parameters loaded from: " << argv[1] << "\n";
    }
    else
    {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <dicom_dir|cfg_file> [output_prefix]\n";
            return 1;
        }
        dicomDir     = argv[1];
        if (argc > 2) outputPrefix = argv[2];
    }

    std::cout << "DICOM directory : " << dicomDir     << "\n";
    std::cout << "Output prefix   : " << outputPrefix << "\n";

    // -------------------------------------------------------------------------
    // 2. Discover and load DICOM series
    // -------------------------------------------------------------------------
    using PixelType    = int16_t;
    using ImageType    = itk::Image<PixelType, 3>;
    using ReaderType   = itk::ImageSeriesReader<ImageType>;
    using NamesGenType = itk::GDCMSeriesFileNames;
    using GDCMIOType   = itk::GDCMImageIO;

    auto namesGen = NamesGenType::New();
    namesGen->SetDirectory(dicomDir);
    namesGen->SetUseSeriesDetails(true);
    namesGen->SetLoadSequences(false);
    namesGen->SetLoadPrivateTags(false);

    const auto &allUIDs = namesGen->GetSeriesUIDs();
    if (allUIDs.empty())
        throw std::runtime_error("No DICOM series found in: " + dicomDir);

    if (seriesUID.empty())
    {
        seriesUID = allUIDs.begin()->c_str();
        if (allUIDs.size() > 1)
            std::cout << "Multiple series found; using first: " << seriesUID << "\n";
    }

    const auto &fileNames = namesGen->GetFileNames(seriesUID.c_str());
    std::cout << "Series UID      : " << seriesUID << "\n";
    std::cout << "Slices found    : " << fileNames.size() << "\n";

    auto gdcmIO = GDCMIOType::New();
    auto reader = ReaderType::New();
    reader->SetImageIO(gdcmIO);
    reader->SetFileNames(fileNames);
    reader->ForceOrthogonalDirectionOff();
    reader->Update();

    ImageType::Pointer huImage = reader->GetOutput();
    huImage->DisconnectPipeline();

    const auto &size    = huImage->GetLargestPossibleRegion().GetSize();
    const auto &spacing = huImage->GetSpacing();  // mm

    const size_t nx = size[0];
    const size_t ny = size[1];
    const size_t nz = size[2];
    const double sx_mm = spacing[0];
    const double sy_mm = spacing[1];
    const double sz_mm = spacing[2];

    std::cout << "Volume size     : " << nx << " x " << ny << " x " << nz << "\n";
    std::cout << "Voxel spacing   : " << sx_mm << " x " << sy_mm << " x " << sz_mm << " mm\n";

    // -------------------------------------------------------------------------
    // 3. Build output directory and base path
    // -------------------------------------------------------------------------
    const std::string dimTag = std::to_string(nx) + "x" +
                               std::to_string(ny) + "x" +
                               std::to_string(nz);

    const std::string outDir  = "./output/" + outputPrefix + "_vox_" + dimTag + "/";
    fs::create_directories(outDir);
    const std::string outBase = outDir + outputPrefix + "_vox_";
    std::cout << "Output folder   : " << outDir << "\n";

    // -------------------------------------------------------------------------
    // 4. HU thresholding → 5-label buffer
    // -------------------------------------------------------------------------
    const size_t totalVox = nx * ny * nz;
    std::vector<uint8_t> labelBuf(totalVox);

    {
        itk::ImageRegionConstIterator<ImageType> it(huImage,
                                                    huImage->GetLargestPossibleRegion());
        size_t idx = 0;
        for (it.GoToBegin(); !it.IsAtEnd(); ++it, ++idx)
        {
            const int16_t hu = it.Get();
            if      (hu < thr_air_fat)        labelBuf[idx] = 0;
            else if (hu < thr_fat_soft)       labelBuf[idx] = 1;
            else if (hu < thr_soft_spongiosa) labelBuf[idx] = 2;
            else if (hu < thr_spongiosa_cort) labelBuf[idx] = 3;
            else                              labelBuf[idx] = 4;
        }
    }

    std::cout << "HU thresholding done (labels 0-4).\n";

    // -------------------------------------------------------------------------
    // 5. Implant cylinder override (axis along Z)
    //    Coordinates are in mm from volume origin (voxel corner of voxel [0,0,0])
    // -------------------------------------------------------------------------
    const bool hasImplant = !implants.empty();
    for (size_t impIdx = 0; impIdx < implants.size(); ++impIdx)
    {
        const auto &imp = implants[impIdx];
        const double r2    = imp.radius_mm * imp.radius_mm;
        const double halfH = imp.height_mm / 2.0;
        size_t implantVoxels = 0;

        for (size_t k = 0; k < nz; ++k)
        {
            const double vz = (k + 0.5) * sz_mm;
            if (std::abs(vz - imp.cz_mm) > halfH) continue;

            for (size_t j = 0; j < ny; ++j)
            {
                const double vy = (j + 0.5) * sy_mm;
                const double dy = vy - imp.cy_mm;

                for (size_t i = 0; i < nx; ++i)
                {
                    const double vx = (i + 0.5) * sx_mm;
                    const double dx = vx - imp.cx_mm;

                    if (dx*dx + dy*dy <= r2)
                    {
                        labelBuf[k * ny * nx + j * nx + i] = 5;
                        ++implantVoxels;
                    }
                }
            }
        }
        std::cout << "Implant[" << impIdx << "]: centre=("
                  << imp.cx_mm << ", " << imp.cy_mm << ", " << imp.cz_mm << ") mm"
                  << "  r=" << imp.radius_mm << " mm"
                  << "  h=" << imp.height_mm << " mm"
                  << "  -> " << implantVoxels << " voxels labelled 5\n";
    }

    // -------------------------------------------------------------------------
    // 6. Crop cylinder mask (zero everything outside, axis along Z)
    // -------------------------------------------------------------------------
    if (crop_cylinder_enable)
    {
        const double r2 = crop_cylinder_radius_mm * crop_cylinder_radius_mm;
        size_t zeroedVoxels = 0;

        for (size_t k = 0; k < nz; ++k)
            for (size_t j = 0; j < ny; ++j)
            {
                const double vy = (j + 0.5) * sy_mm;
                const double dy = vy - crop_cylinder_cy_mm;
                for (size_t i = 0; i < nx; ++i)
                {
                    const double vx = (i + 0.5) * sx_mm;
                    const double dx = vx - crop_cylinder_cx_mm;
                    if (dx*dx + dy*dy > r2)
                    {
                        labelBuf[k * ny * nx + j * nx + i] = 0;
                        ++zeroedVoxels;
                    }
                }
            }
        std::cout << "Crop cylinder: centre=(" << crop_cylinder_cx_mm << ", "
                  << crop_cylinder_cy_mm << ") mm"
                  << "  r=" << crop_cylinder_radius_mm << " mm"
                  << "  -> " << zeroedVoxels << " voxels zeroed\n";
    }

    // -------------------------------------------------------------------------
    // 7. Label counts
    // -------------------------------------------------------------------------
    {
        size_t counts[6] = {0};
        for (uint8_t v : labelBuf) if (v < 6) ++counts[v];
        const char *names[] = {"air", "fat", "soft", "spongiosa", "cortical", "implant"};
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Label distribution:\n";
        for (int l = 0; l < 6; ++l)
            if (counts[l] > 0)
                std::cout << "  " << l << " (" << names[l] << "): " << counts[l]
                          << "  (" << 100.0 * counts[l] / totalVox << " %)\n";
    }

    // -------------------------------------------------------------------------
    // 8. Write uint8 RAW label file + companion .txt
    // -------------------------------------------------------------------------
    const std::string labelTag = std::string(hasImplant ? "5labels_implant_" : "5labels_")
                               + (crop_cylinder_enable ? "reconCylinder_" : "");

    const std::string rawBase = outBase + labelTag + dimTag;
    const std::string rawPath = rawBase + ".raw";

    {
        std::ofstream f(rawPath, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot write label file: " + rawPath);
        f.write(reinterpret_cast<const char *>(labelBuf.data()),
                static_cast<std::streamsize>(totalVox));
    }
    std::cout << (hasImplant ? "6" : "5") << "-label phantom written: " << rawPath << "\n";

    writeInfoFile(rawBase,
                  nx, ny, nz,
                  sx_mm / 10.0,   // mm -> cm
                  sy_mm / 10.0,
                  sz_mm / 10.0);

    std::cout << "Done.\n";
    return 0;
}
