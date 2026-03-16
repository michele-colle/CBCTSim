// hu_to_4labels.cpp
// Reads a HU int16 raw phantom + its MC-GPU info .txt, applies HU thresholding
// to produce a 4-label phantom: 0=air, 1=fat, 2=soft tissue, 3=bone.
//
// Usage:
//   hu_to_4labels <info_txt> <hu_raw_int16> <output_base>
//                 [hu_air_fat=-500] [hu_fat_soft=-100] [hu_soft_bone=200]
//
// The info .txt must follow the MC-GPU voxelized geometry format produced by
// mcrp_to_vox (offset line, nx ny nz line, spacing line).

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Default HU thresholds (widely used clinical values)
static constexpr int16_t DEFAULT_THR_AIR_FAT   = -500;
static constexpr int16_t DEFAULT_THR_FAT_SOFT  = -100;
static constexpr int16_t DEFAULT_THR_SOFT_BONE =  200;

struct PhantomInfo {
    double offset[3];   // cm
    size_t nx, ny, nz;
    double spacing[3];  // cm
};

// Parse the MC-GPU geometry info file written by LabelsToRawFile / mcrp_to_vox.
// Expected non-comment, non-empty lines in order:
//   1: phantom/filename.raw
//   2: offset_x  offset_y  offset_z
//   3: nx  ny  nz
//   4: sx  sy  sz
//   5: 0 0 0
PhantomInfo parseInfoFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open info file: " + path);

    PhantomInfo info{};
    std::string line;
    int dataLine = 0;

    while (std::getline(f, line)) {
        // skip blank and section-header lines
        if (line.empty() || line[0] == '#') continue;
        // strip inline comment
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);

        std::istringstream ss(line);
        switch (dataLine) {
            case 0: /* filename reference, skip */ break;
            case 1: ss >> info.offset[0] >> info.offset[1] >> info.offset[2]; break;
            case 2: ss >> info.nx >> info.ny >> info.nz; break;
            case 3: ss >> info.spacing[0] >> info.spacing[1] >> info.spacing[2]; break;
            default: break;
        }
        ++dataLine;
        if (dataLine > 4) break;
    }

    if (info.nx == 0 || info.ny == 0 || info.nz == 0)
        throw std::runtime_error("Failed to parse valid dimensions from: " + path);

    return info;
}

// Write a MC-GPU geometry info file compatible with the rest of the pipeline.
void writeInfoFile(const std::string& base, const PhantomInfo& info)
{
    std::string txtPath = base + ".txt";
    std::ofstream f(txtPath);
    if (!f) throw std::runtime_error("Cannot write info file: " + txtPath);

    std::string fname = fs::path(base).filename().string();

    f << std::fixed << std::setprecision(3);
    f << "#[SECTION VOXELIZED GEOMETRY FILE v.2017-07-26]\n";
    f << "phantom/" << fname << ".raw     # VOXEL GEOMETRY FILE (penEasy 2008 format; .gz accepted)\n";
    f << " " << info.offset[0] << "  " << info.offset[1] << "  " << info.offset[2]
      << "              # OFFSET OF THE VOXEL GEOMETRY [cm]\n";
    f << " " << info.nx << " " << info.ny << " " << info.nz
      << "                 # NUMBER OF VOXELS\n";
    f << " " << info.spacing[0] << " " << info.spacing[1] << " " << info.spacing[2]
      << "           # VOXEL SIZES [cm]\n";
    f << " 0 0 0                          # SIZE OF LOW RESOLUTION VOXELS\n";

    std::cout << "Info file written: " << txtPath << "\n";
}

int main(int argc, char* argv[])
{
    if (argc < 4) {
        std::cerr
            << "Usage: " << argv[0]
            << " <info_txt> <hu_raw_int16> <output_base>"
            << " [hu_air_fat=" << DEFAULT_THR_AIR_FAT << "]"
            << " [hu_fat_soft=" << DEFAULT_THR_FAT_SOFT << "]"
            << " [hu_soft_bone=" << DEFAULT_THR_SOFT_BONE << "]\n"
            << "\nOutputs 8-bit label phantom with:\n"
            << "  0 = air        (HU < hu_air_fat)\n"
            << "  1 = fat        (hu_air_fat  <= HU < hu_fat_soft)\n"
            << "  2 = soft tissue(hu_fat_soft <= HU < hu_soft_bone)\n"
            << "  3 = bone       (HU >= hu_soft_bone)\n";
        return 1;
    }

    const std::string infoPath   = argv[1];
    const std::string huRawPath  = argv[2];
    const std::string outputBase = argv[3];

    const int16_t thrAirFat  = (argc > 4) ? static_cast<int16_t>(std::stoi(argv[4])) : DEFAULT_THR_AIR_FAT;
    const int16_t thrFatSoft = (argc > 5) ? static_cast<int16_t>(std::stoi(argv[5])) : DEFAULT_THR_FAT_SOFT;
    const int16_t thrSoftBone= (argc > 6) ? static_cast<int16_t>(std::stoi(argv[6])) : DEFAULT_THR_SOFT_BONE;

    std::cout << "HU thresholds:\n"
              << "  0 air  : HU < "          << thrAirFat   << "\n"
              << "  1 fat  : "  << thrAirFat  << " <= HU < " << thrFatSoft  << "\n"
              << "  2 soft : "  << thrFatSoft << " <= HU < " << thrSoftBone << "\n"
              << "  3 bone : HU >= "          << thrSoftBone << "\n\n";

    // -------------------------------------------------------------------------
    // 1. Parse geometry from info file
    // -------------------------------------------------------------------------
    const auto info = parseInfoFile(infoPath);
    const size_t totalVoxels = info.nx * info.ny * info.nz;

    std::cout << "Phantom size: " << info.nx << " x " << info.ny << " x " << info.nz
              << "  (" << totalVoxels << " voxels)\n"
              << "Spacing [cm]: " << info.spacing[0] << " " << info.spacing[1] << " " << info.spacing[2] << "\n\n";

    // -------------------------------------------------------------------------
    // 2. Read HU int16 raw data
    // -------------------------------------------------------------------------
    std::vector<int16_t> huData(totalVoxels);
    {
        std::ifstream f(huRawPath, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open HU file: " + huRawPath);
        f.read(reinterpret_cast<char*>(huData.data()),
               static_cast<std::streamsize>(totalVoxels * sizeof(int16_t)));
        const size_t bytesRead = static_cast<size_t>(f.gcount());
        if (bytesRead != totalVoxels * sizeof(int16_t)) {
            throw std::runtime_error(
                "HU file size mismatch: expected " +
                std::to_string(totalVoxels * sizeof(int16_t)) +
                " bytes, got " + std::to_string(bytesRead));
        }
    }
    std::cout << "HU data loaded from: " << huRawPath << "\n";

    // -------------------------------------------------------------------------
    // 3. Apply HU thresholding → 4 labels
    // -------------------------------------------------------------------------
    std::vector<uint8_t> labelData(totalVoxels);
    size_t counts[4] = {0, 0, 0, 0};

    for (size_t i = 0; i < totalVoxels; ++i) {
        const int16_t hu = huData[i];
        uint8_t lbl;
        if      (hu < thrAirFat)   lbl = 0;
        else if (hu < thrFatSoft)  lbl = 1;
        else if (hu < thrSoftBone) lbl = 2;
        else                       lbl = 3;
        labelData[i] = lbl;
        ++counts[lbl];
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Label counts:\n"
              << "  0 (air)  : " << counts[0] << "  (" << 100.0 * counts[0] / totalVoxels << " %)\n"
              << "  1 (fat)  : " << counts[1] << "  (" << 100.0 * counts[1] / totalVoxels << " %)\n"
              << "  2 (soft) : " << counts[2] << "  (" << 100.0 * counts[2] / totalVoxels << " %)\n"
              << "  3 (bone) : " << counts[3] << "  (" << 100.0 * counts[3] / totalVoxels << " %)\n\n";

    // -------------------------------------------------------------------------
    // 4. Write 8-bit label raw file + info .txt
    // -------------------------------------------------------------------------
    const std::string dimSuffix = "_" +
        std::to_string(info.nx) + "x" +
        std::to_string(info.ny) + "x" +
        std::to_string(info.nz);

    const std::string rawPath  = outputBase + dimSuffix + ".raw";
    const std::string infoBase = outputBase + dimSuffix;

    {
        std::ofstream f(rawPath, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot write label file: " + rawPath);
        f.write(reinterpret_cast<const char*>(labelData.data()),
                static_cast<std::streamsize>(totalVoxels));
    }
    std::cout << "Label raw written: " << rawPath << "\n";

    writeInfoFile(infoBase, info);

    std::cout << "\nDone.\n";
    return 0;
}
