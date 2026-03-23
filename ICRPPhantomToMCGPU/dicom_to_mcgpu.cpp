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
// Stretcher: two adjacent boxes in the XZ plane (full Z extent).
// carbon (label 10) occupies the -Y half of the assembly (structural side).
// foam   (label 11) occupies the +Y half (patient-contact side).
// cx_mm / cy_mm are coordinates of the assembly centre from the isocenter.
// ---------------------------------------------------------------------------
struct Stretcher {
    double cx_mm;                // X centre from isocenter [mm]
    double cy_mm;                // Y centre from isocenter [mm]
    double width_mm;             // full X extent [mm]
    double carbon_thickness_mm;  // Y thickness of carbon layer (label 10)
    double foam_thickness_mm;    // Y thickness of foam layer   (label 11)
};

// ---------------------------------------------------------------------------
// Write MC-GPU [SECTION VOXELIZED GEOMETRY FILE] companion .txt
// (same format produced by mcrp_to_vox)
// ---------------------------------------------------------------------------
static void writeInfoFile(const std::string &base,
                          size_t nx, size_t ny, size_t nz,
                          double sx_cm, double sy_cm, double sz_cm,
                          double shift_x_cm, double shift_y_cm, double shift_z_cm)
{
    const std::string txtPath = base + ".txt";
    std::ofstream f(txtPath);
    if (!f) throw std::runtime_error("Cannot write info file: " + txtPath);

    const std::string fname = fs::path(base).filename().string();

    f << std::fixed << std::setprecision(3);
    f << "#[SECTION VOXELIZED GEOMETRY FILE v.2017-07-26]\n";
    f << "phantom/" << fname << ".raw"
      << "     # VOXEL GEOMETRY FILE (penEasy 2008 format; .gz accepted)\n";
    // centred offset + user shift
    f << " " << -(static_cast<double>(nx) * sx_cm / 2.0) + shift_x_cm
      << "  " << -(static_cast<double>(ny) * sy_cm / 2.0) + shift_y_cm
      << "  " << -(static_cast<double>(nz) * sz_cm / 2.0) + shift_z_cm
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
    // 0. Print working directory and its contents
    // -------------------------------------------------------------------------
    std::cout << "Working directory: " << fs::current_path().string() << "\n";
    std::cout << "Contents:\n";
    for (const auto &entry : fs::directory_iterator(fs::current_path()))
        std::cout << "  " << entry.path().filename().string()
                  << (entry.is_directory() ? "/" : "") << "\n";
    std::cout << "\n";

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
    // geometry shift (mm, added to centred-corner offset in the .txt file)
    double shift_x_mm = 0.0;
    double shift_y_mm = 0.0;
    double shift_z_mm = 0.0;
    // implant cylinders
    std::vector<ImplantCylinder> implants;
    // stretcher
    bool      stretcher_enable = false;
    Stretcher stretcher        = {0.0, 0.0, 600.0, 3.0, 60.0};
    // MC-GPU .in template
    std::string mcgpu_in_template;
    std::string mcgpu_output_name;   // base path for the detector image output
    int         mcgpu_det_nx = 512;
    int         mcgpu_det_nz = 512;

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
        std::replace(dicomDir.begin(), dicomDir.end(), '\\', '/');
        outputPrefix = getS("output_prefix", outputPrefix);
        seriesUID    = getS("series_uid",    seriesUID);

        thr_air_fat        = static_cast<int16_t>(getD("thr_air_fat",        thr_air_fat));
        thr_fat_soft       = static_cast<int16_t>(getD("thr_fat_soft",       thr_fat_soft));
        thr_soft_spongiosa = static_cast<int16_t>(getD("thr_soft_spongiosa", thr_soft_spongiosa));
        thr_spongiosa_cort = static_cast<int16_t>(getD("thr_spongiosa_cort", thr_spongiosa_cort));

        shift_x_mm = getD("shift_x_mm", shift_x_mm);
        shift_y_mm = getD("shift_y_mm", shift_y_mm);
        shift_z_mm = getD("shift_z_mm", shift_z_mm);

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
        stretcher_enable = cfg.count("stretcher_enable")
            ? (cfg["stretcher_enable"] == "1" || cfg["stretcher_enable"] == "true")
            : stretcher_enable;
        stretcher.cx_mm                = getD("stretcher_cx_mm",                stretcher.cx_mm);
        stretcher.cy_mm                = getD("stretcher_cy_mm",                stretcher.cy_mm);
        stretcher.width_mm             = getD("stretcher_width_mm",             stretcher.width_mm);
        stretcher.carbon_thickness_mm  = getD("stretcher_carbon_thickness_mm",  stretcher.carbon_thickness_mm);
        stretcher.foam_thickness_mm    = getD("stretcher_foam_thickness_mm",    stretcher.foam_thickness_mm);

        mcgpu_in_template  = getS("mcgpu_in_template",  mcgpu_in_template);
        mcgpu_output_name  = getS("mcgpu_output_name",  mcgpu_output_name);
        mcgpu_det_nx       = static_cast<int>(getD("mcgpu_det_nx", mcgpu_det_nx));
        mcgpu_det_nz       = static_cast<int>(getD("mcgpu_det_nz", mcgpu_det_nz));

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

    size_t nx = size[0];
    size_t ny = size[1];
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

    // output_prefix may contain leading "../" segments to navigate up from dicomDir's parent.
    // The filename stem is the last component only; the rest is directory navigation.
    const fs::path   prefixPath = fs::path(outputPrefix);
    const std::string prefixStem = prefixPath.filename().string();
    const fs::path outDirPath = (fs::path(dicomDir).parent_path() /
                                prefixPath.parent_path() /
                                (prefixStem + "_vox_" + dimTag)).lexically_normal();
    const std::string outDir  = outDirPath.string() + "/";
    fs::create_directories(outDirPath);
    const std::string outBase = outDir + prefixStem + "_vox_";
    std::cout << "Output folder   : " << outDir << "\n";

    // -------------------------------------------------------------------------
    // 4. HU thresholding → 5-label buffer
    // -------------------------------------------------------------------------
    size_t totalVox = nx * ny * nz;
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
    // 6.5 Stretcher: two adjacent boxes (foam -Y / carbon +Y) in the XZ plane.
    //     The volume is zero-padded in X and/or Y if the stretcher falls outside
    //     the current bounds.  The geometry shift is updated accordingly so the
    //     isocenter stays fixed.
    // -------------------------------------------------------------------------
    if (stretcher_enable)
    {
        // Convert stretcher centre from isocenter coords → volume-origin coords.
        // The volume centre sits at (shift_x_mm, shift_y_mm) in the world frame,
        // so: vol_origin_coord = world_coord + vol_half_size - shift
        double sc_x_vol = stretcher.cx_mm + (static_cast<double>(nx) * sx_mm / 2.0) - shift_x_mm;
        double sc_y_vol = stretcher.cy_mm + (static_cast<double>(ny) * sy_mm / 2.0) - shift_y_mm;

        const double total_t = stretcher.carbon_thickness_mm + stretcher.foam_thickness_mm;
        const double half_w  = stretcher.width_mm / 2.0;
        const double half_t  = total_t / 2.0;

        // Stretcher bounding box in volume-origin coords [mm]
        const double sx_min = sc_x_vol - half_w;
        const double sx_max = sc_x_vol + half_w;
        const double sy_min = sc_y_vol - half_t;
        const double sy_max = sc_y_vol + half_t;

        // Voxels of padding required on each side
        const int pad_xl = (sx_min < 0.0)
            ? static_cast<int>(std::ceil(-sx_min / sx_mm)) : 0;
        const int pad_xr = (sx_max > static_cast<double>(nx) * sx_mm)
            ? static_cast<int>(std::ceil((sx_max - static_cast<double>(nx) * sx_mm) / sx_mm)) : 0;
        const int pad_yb = (sy_min < 0.0)
            ? static_cast<int>(std::ceil(-sy_min / sy_mm)) : 0;
        const int pad_yt = (sy_max > static_cast<double>(ny) * sy_mm)
            ? static_cast<int>(std::ceil((sy_max - static_cast<double>(ny) * sy_mm) / sy_mm)) : 0;

        const size_t new_nx = nx + static_cast<size_t>(pad_xl + pad_xr);
        const size_t new_ny = ny + static_cast<size_t>(pad_yb + pad_yt);

        if (pad_xl || pad_xr || pad_yb || pad_yt)
        {
            std::cout << "Stretcher: padding volume ("
                      << pad_xl << "+" << pad_xr << ") x ("
                      << pad_yb << "+" << pad_yt << ") voxels in X / Y.\n";

            std::vector<uint8_t> newBuf(new_nx * new_ny * nz, 0);
            for (size_t k = 0; k < nz; ++k)
                for (size_t j = 0; j < ny; ++j)
                    for (size_t i = 0; i < nx; ++i)
                        newBuf[k * new_ny * new_nx
                               + (j + static_cast<size_t>(pad_yb)) * new_nx
                               + (i + static_cast<size_t>(pad_xl))]
                            = labelBuf[k * ny * nx + j * nx + i];
            labelBuf = std::move(newBuf);

            // Keep the isocenter fixed: the new volume centre has shifted by
            // (pad_xr - pad_xl)/2 * sx_mm relative to the old centre.
            shift_x_mm += (static_cast<double>(pad_xr) - static_cast<double>(pad_xl)) * sx_mm / 2.0;
            shift_y_mm += (static_cast<double>(pad_yt) - static_cast<double>(pad_yb)) * sy_mm / 2.0;
        }

        // Update stretcher centre for the new (padded) origin and volume dims
        sc_x_vol += static_cast<double>(pad_xl) * sx_mm;
        sc_y_vol += static_cast<double>(pad_yb) * sy_mm;
        nx = new_nx;
        ny = new_ny;
        totalVox = nx * ny * nz;

        // Fill stretcher voxels.
        // carbon (label 10): lower-Y half  [sc_y_vol - half_t,  sc_y_vol - half_t + carbon_t)
        // foam   (label 11): upper-Y half  [sc_y_vol - half_t + carbon_t,  sc_y_vol + half_t)
        const double carb_y_min = sc_y_vol - half_t;
        const double carb_y_max = carb_y_min + stretcher.carbon_thickness_mm;
        const double foam_y_max = sc_y_vol + half_t;
        const double str_x_min  = sc_x_vol - half_w;
        const double str_x_max  = sc_x_vol + half_w;

        size_t foamVox = 0, carbVox = 0;
        for (size_t k = 0; k < nz; ++k)
            for (size_t j = 0; j < ny; ++j)
            {
                const double vy = (static_cast<double>(j) + 0.5) * sy_mm;
                const bool in_carbon = (vy >= carb_y_min && vy <  carb_y_max);
                const bool in_foam   = (vy >= carb_y_max && vy <  foam_y_max);
                if (!in_foam && !in_carbon) continue;

                for (size_t i = 0; i < nx; ++i)
                {
                    const double vx = (static_cast<double>(i) + 0.5) * sx_mm;
                    if (vx < str_x_min || vx >= str_x_max) continue;

                    const size_t idx = k * ny * nx + j * nx + i;
                    if (in_foam)   { labelBuf[idx] = 11; ++foamVox; }
                    else           { labelBuf[idx] = 10; ++carbVox; }
                }
            }

        std::cout << "Stretcher: centre=(" << stretcher.cx_mm << ", " << stretcher.cy_mm
                  << ") mm from isocenter"
                  << "  width=" << stretcher.width_mm << " mm"
                  << "  carbon=" << stretcher.carbon_thickness_mm << " mm (label 10)"
                  << "  foam="   << stretcher.foam_thickness_mm   << " mm (label 11)"
                  << "  -> " << carbVox << " carbon + " << foamVox << " foam voxels\n";
        std::cout << "Updated volume: " << nx << " x " << ny << " x " << nz << "\n";
        std::cout << "Updated shift : (" << shift_x_mm << ", " << shift_y_mm << ", "
                  << shift_z_mm << ") mm\n";
    }

    // -------------------------------------------------------------------------
    // 7. Label counts
    // -------------------------------------------------------------------------
    {
        // Labels 0-5 (tissue/implant) + 10 (carbon) + 11 (foam)
        size_t counts[12] = {0};
        for (uint8_t v : labelBuf) if (v < 12) ++counts[v];
        const char *names[12] = {"air", "fat", "soft", "spongiosa", "cortical", "implant",
                                  "", "", "", "", "carbon", "foam"};
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Label distribution:\n";
        for (int l = 0; l < 12; ++l)
            if (counts[l] > 0 && names[l][0] != '\0')
                std::cout << "  " << l << " (" << names[l] << "): " << counts[l]
                          << "  (" << 100.0 * counts[l] / totalVox << " %)\n";
    }

    // -------------------------------------------------------------------------
    // 8. Write uint8 RAW label file + companion .txt
    // -------------------------------------------------------------------------
    const std::string finalDimTag = std::to_string(nx) + "x" +
                                    std::to_string(ny) + "x" +
                                    std::to_string(nz);
    const std::string labelTag = std::string(hasImplant      ? "implant_"     : "")
                               + (stretcher_enable           ? "stretcher_"   : "")
                               + (crop_cylinder_enable       ? "reconCylinder_" : "")
                               + (hasImplant || stretcher_enable ? "labels_" : "5labels_");

    const std::string rawBase = outBase + labelTag + finalDimTag;
    const std::string rawPath = rawBase + ".raw";

    {
        std::ofstream f(rawPath, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot write label file: " + rawPath);
        f.write(reinterpret_cast<const char *>(labelBuf.data()),
                static_cast<std::streamsize>(totalVox));
    }
    std::cout << "Label phantom written: " << rawPath << "\n";

    writeInfoFile(rawBase,
                  nx, ny, nz,
                  sx_mm / 10.0,      // mm -> cm
                  sy_mm / 10.0,
                  sz_mm / 10.0,
                  shift_x_mm / 10.0, // mm -> cm
                  shift_y_mm / 10.0,
                  shift_z_mm / 10.0);

    // -------------------------------------------------------------------------
    // 9. Write MC-GPU .in file (based on template, geometry section replaced)
    // -------------------------------------------------------------------------
    if (!mcgpu_in_template.empty())
    {
        std::ifstream tmpl(mcgpu_in_template);
        if (!tmpl)
        {
            std::cerr << "Warning: cannot open MC-GPU template: " << mcgpu_in_template << "\n";
        }
        else
        {
            const std::string inPath = outDir + "CBCT.in";
            std::ofstream out(inPath);
            if (!out) throw std::runtime_error("Cannot write .in file: " + inPath);

            const double sx_cm = sx_mm / 10.0, sy_cm = sy_mm / 10.0, sz_cm = sz_mm / 10.0;
            const double off_x = -(static_cast<double>(nx) * sx_cm / 2.0) + shift_x_mm / 10.0;
            const double off_y = -(static_cast<double>(ny) * sy_cm / 2.0) + shift_y_mm / 10.0;
            const double off_z = -(static_cast<double>(nz) * sz_cm / 2.0) + shift_z_mm / 10.0;
            const std::string absRawPath = fs::absolute(rawPath).string();

            std::string line;
            int skipLines = 0;
            while (std::getline(tmpl, line))
            {
                if (line.find("#[SECTION IMAGE DETECTOR") != std::string::npos)
                {
                    out << line << "\n";
                    const std::string detTag = std::to_string(mcgpu_det_nx) + "x"
                                             + std::to_string(mcgpu_det_nz)+"x2float";
                    out << mcgpu_output_name << detTag
                        << "   # OUTPUT IMAGE FILE NAME\n";
                    out << mcgpu_det_nx << "      " << mcgpu_det_nz
                        << "                  # NUMBER OF PIXELS IN THE IMAGE: Nx Nz\n";
                    skipLines = 2;
                }
                else if (line.find("#[SECTION VOXELIZED GEOMETRY FILE") != std::string::npos)
                {
                    out << line << "\n";
                    out << std::fixed << std::setprecision(3);
                    out << absRawPath << "     # VOXEL GEOMETRY FILE (penEasy 2008 format; .gz accepted)\n";
                    out << " " << off_x << "  " << off_y << "  " << off_z
                        << "              # OFFSET OF THE VOXEL GEOMETRY [cm]\n";
                    out << " " << nx << " " << ny << " " << nz
                        << "                 # NUMBER OF VOXELS\n";
                    out << " " << sx_cm << " " << sy_cm << " " << sz_cm
                        << "           # VOXEL SIZES [cm]\n";
                    out << " 0 0 0                          # SIZE OF LOW RESOLUTION VOXELS\n";
                    skipLines = 5;  // skip the 5 original geometry lines
                }
                else if (skipLines > 0)
                {
                    --skipLines;
                }
                else
                {
                    out << line << "\n";
                }
            }
            std::cout << "MC-GPU .in file written: " << inPath << "\n";
        }
    }

    std::cout << "Done.\n";
    return 0;
}
