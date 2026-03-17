#include <algorithm>
#include <cmath>
#include <cfloat>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

#include "TETModelImport.hh"
#include "G4DatReader.hpp"
#include "HUDicomExporter.hpp"
#include "ImageUtils.hpp"
#include "MCRPMaterialMap.hpp"

#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Tet.hh"
#include "G4UIExecutive.hh"
#include "G4RunManager.hh"
#include "G4EmCalculator.hh"
#include "G4NistManager.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4Gamma.hh"
#include "FTFP_BERT.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"

#include <itkImage.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>

// ---------------------------------------------------------------------------
// Minimal Geant4 environment needed to drive G4EmCalculator
// (same pattern as G4MaterialToHU)
// ---------------------------------------------------------------------------
namespace
{
    class MiniDetector : public G4VUserDetectorConstruction
    {
    public:
        G4VPhysicalVolume *Construct() override
        {
            G4Material *air = G4NistManager::Instance()->FindOrBuildMaterial("G4_AIR");
            G4Box *box = new G4Box("World", 1 * m, 1 * m, 1 * m);
            G4LogicalVolume *lv = new G4LogicalVolume(box, air, "World");
            return new G4PVPlacement(nullptr, G4ThreeVector(), lv, "World", nullptr, false, 0);
        }
    };
    class MiniGun : public G4VUserPrimaryGeneratorAction
    {
    public:
        void GeneratePrimaries(G4Event *event) override
        {
            G4ParticleGun gun(1);
            gun.SetParticleDefinition(G4Gamma::Definition());
            gun.GeneratePrimaryVertex(event);
        }
    };
    class MiniAction : public G4VUserActionInitialization
    {
    public:
        void Build() const override { SetUserAction(new MiniGun()); }
    };
}

// ---------------------------------------------------------------------------
// Config-file parser: reads "key = value" lines, ignores # comments
// ---------------------------------------------------------------------------
static std::map<std::string, std::string> loadCfg(const std::string &path)
{
    std::map<std::string, std::string> cfg;
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open config file: " + path);
    std::string line;
    while (std::getline(f, line))
    {
        // strip comment
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

int main(int argc, char **argv)
{
    // ---------------------------------------------------------------------
    // 1. Parse parameters — from a .cfg file or positional args
    // ---------------------------------------------------------------------

    // defaults
    std::string phantomName   = "MRCP_AF";
    double voxelSize_mm       = 0.4;
    double zStart_mm          = 0.0;
    double zEnd_mm            = -1.0;   // negative = full extent
    double xStart_mm          = 0.0;
    double xEnd_mm            = -1.0;
    double yStart_mm          = 0.0;
    double yEnd_mm            = -1.0;
    // HU thresholds
    int16_t thr_air_fat        = -500;
    int16_t thr_fat_soft       =  -50;
    int16_t thr_soft_spongiosa =  200;
    int16_t thr_spongiosa_cort =  800;
    // crop cylinder — labels outside are zeroed; center = volume center + shift
    bool   crop_cylinder_enable    = false;
    double crop_cylinder_radius_mm = 100.0;
    double crop_cylinder_cx_mm     = 0.0;   // absolute coords from bbMin corner [mm]
    double crop_cylinder_cy_mm     = 0.0;
    // implant cylinder (axis along Z). radius <= 0 disables it.
    // coordinates are absolute in the voxel volume [mm from bbMin]
    double implant_cx_mm      = 275.5;
    double implant_cy_mm      = 74.5;
    double implant_cz_mm      = 1462.0;
    double implant_radius_mm  = 2.5;
    double implant_height_mm  = 22.0;

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
        phantomName      = getS("phantom_name",      phantomName);
        voxelSize_mm     = getD("voxel_size_mm",     voxelSize_mm);
        xStart_mm        = getD("x_start_mm",        xStart_mm);
        xEnd_mm          = getD("x_end_mm",          xEnd_mm);
        yStart_mm        = getD("y_start_mm",        yStart_mm);
        yEnd_mm          = getD("y_end_mm",          yEnd_mm);
        zStart_mm        = getD("z_start_mm",        zStart_mm);
        zEnd_mm          = getD("z_end_mm",          zEnd_mm);
        implant_cx_mm    = getD("implant_cx_mm",     implant_cx_mm);
        implant_cy_mm    = getD("implant_cy_mm",     implant_cy_mm);
        implant_cz_mm    = getD("implant_cz_mm",     implant_cz_mm);
        implant_radius_mm   = getD("implant_radius_mm",    implant_radius_mm);
        implant_height_mm   = getD("implant_height_mm",    implant_height_mm);
        crop_cylinder_enable    = cfg.count("crop_cylinder_enable")
                                  ? (cfg["crop_cylinder_enable"] == "1" || cfg["crop_cylinder_enable"] == "true")
                                  : crop_cylinder_enable;
        crop_cylinder_radius_mm = getD("crop_cylinder_radius_mm", crop_cylinder_radius_mm);
        crop_cylinder_cx_mm     = getD("crop_cylinder_cx_mm",     crop_cylinder_cx_mm);
        crop_cylinder_cy_mm     = getD("crop_cylinder_cy_mm",     crop_cylinder_cy_mm);
        thr_air_fat         = static_cast<int16_t>(getD("thr_air_fat",        thr_air_fat));
        thr_fat_soft        = static_cast<int16_t>(getD("thr_fat_soft",       thr_fat_soft));
        thr_soft_spongiosa  = static_cast<int16_t>(getD("thr_soft_spongiosa", thr_soft_spongiosa));
        thr_spongiosa_cort  = static_cast<int16_t>(getD("thr_spongiosa_cort", thr_spongiosa_cort));
        std::cout << "Parameters loaded from: " << argv[1] << std::endl;
    }
    else
    {
        // positional args (legacy / manual use)
        if (argc > 1)  phantomName      = argv[1];
        if (argc > 2)  voxelSize_mm     = std::stod(argv[2]);
        if (argc > 3)  zStart_mm        = std::stod(argv[3]);
        if (argc > 4)  zEnd_mm          = std::stod(argv[4]);
        if (argc > 5)  xStart_mm        = std::stod(argv[5]);
        if (argc > 6)  xEnd_mm          = std::stod(argv[6]);
        if (argc > 7)  yStart_mm        = std::stod(argv[7]);
        if (argc > 8)  yEnd_mm          = std::stod(argv[8]);
        if (argc > 9)  implant_cx_mm    = std::stod(argv[9]);
        if (argc > 10) implant_cy_mm    = std::stod(argv[10]);
        if (argc > 11) implant_cz_mm    = std::stod(argv[11]);
        if (argc > 12) implant_radius_mm= std::stod(argv[12]);
        if (argc > 13) implant_height_mm= std::stod(argv[13]);
    }

    // outputPrefix: cfg stem when using a cfg file, otherwise phantom name
    const std::string outputPrefix = usingCfg
        ? std::filesystem::path(argv[1]).stem().string()
        : phantomName;
    std::string outputBase = "./output/" + outputPrefix + "_vox_";

    std::cout << "Using phantom: " << phantomName << std::endl;
    std::cout << "Target voxel size: " << voxelSize_mm << " mm" << std::endl;

    // ---------------------------------------------------------------------
    // 1b. Load material map (MRCP IDs → sequential uint8 indices)
    // ---------------------------------------------------------------------
    MCRPMaterialMap matMap;
    {
        std::string mapDir = "./data/mcgpu_mcrp_materials/" + phantomName;
        matMap.load(mapDir);
    }

    // ---------------------------------------------------------------------
    // 2. Load tetrahedral phantom (MRCP) via TETModelImport
    // ---------------------------------------------------------------------
    G4UIExecutive *ui = nullptr;
    TETModelImport tetImport(phantomName, ui, "./phantoms");

    G4ThreeVector bbMin = tetImport.GetPhantomBoxMin();
    G4ThreeVector bbMax = tetImport.GetPhantomBoxMax();

    const G4double voxelSize = voxelSize_mm * mm;

    const G4double lenX = bbMax.x() - bbMin.x();
    const G4double lenY = bbMax.y() - bbMin.y();
    const G4double lenZ = bbMax.z() - bbMin.z();

    const G4int nx = static_cast<G4int>(std::ceil(lenX / voxelSize));
    const G4int ny = static_cast<G4int>(std::ceil(lenY / voxelSize));
    const G4int nz = static_cast<G4int>(std::ceil(lenZ / voxelSize));

    // Resolve defaults (negative = full extent) and clamp to phantom bounds
    if (xEnd_mm < 0.0) xEnd_mm = lenX / mm;
    if (yEnd_mm < 0.0) yEnd_mm = lenY / mm;
    if (zEnd_mm < 0.0) zEnd_mm = lenZ / mm;
    if (xStart_mm < 0.0) xStart_mm = 0.0;
    if (yStart_mm < 0.0) yStart_mm = 0.0;

    xStart_mm = std::clamp(xStart_mm, 0.0, lenX / mm);
    xEnd_mm   = std::clamp(xEnd_mm,   xStart_mm, lenX / mm);
    yStart_mm = std::clamp(yStart_mm, 0.0, lenY / mm);
    yEnd_mm   = std::clamp(yEnd_mm,   yStart_mm, lenY / mm);
    zStart_mm = std::clamp(zStart_mm, 0.0, lenZ / mm);
    zEnd_mm   = std::clamp(zEnd_mm,   zStart_mm, lenZ / mm);

    const G4int iStart   = static_cast<G4int>(std::floor(xStart_mm / voxelSize_mm));
    const G4int iEnd     = static_cast<G4int>(std::ceil (xEnd_mm   / voxelSize_mm));
    const G4int nx_slice = std::max(1, std::min(iEnd, nx) - iStart);

    const G4int jStart   = static_cast<G4int>(std::floor(yStart_mm / voxelSize_mm));
    const G4int jEnd     = static_cast<G4int>(std::ceil (yEnd_mm   / voxelSize_mm));
    const G4int ny_slice = std::max(1, std::min(jEnd, ny) - jStart);

    const G4int kStart   = static_cast<G4int>(std::floor(zStart_mm / voxelSize_mm));
    const G4int kEnd     = static_cast<G4int>(std::ceil (zEnd_mm   / voxelSize_mm));
    const G4int nz_slice = std::max(1, std::min(kEnd, nz) - kStart);

    // Build output folder <phantomName>_vox_NxNxN and redirect outputBase into it.
    {
        const std::string dimTag = std::to_string(nx_slice) + "x" +
                                   std::to_string(ny_slice) + "x" +
                                   std::to_string(nz_slice);
        const std::string outDir = "./output/" + outputPrefix + "_vox_" + dimTag + "/";
        std::filesystem::create_directories(outDir);
        outputBase = outDir + outputPrefix + "_vox_";
        std::cout << "Output folder: " << outDir << std::endl;
    }

    std::cout << "Bounding box size [mm]: "
              << lenX / mm << " x " << lenY / mm << " x " << lenZ / mm << std::endl;
    std::cout << "Voxel grid size (full): "
              << nx << " x " << ny << " x " << nz << std::endl;
    std::cout << "X ROI: " << xStart_mm << " – " << xEnd_mm
              << " mm  (i " << iStart << " – " << iStart + nx_slice - 1 << ")" << std::endl;
    std::cout << "Y ROI: " << yStart_mm << " – " << yEnd_mm
              << " mm  (j " << jStart << " – " << jStart + ny_slice - 1 << ")" << std::endl;
    std::cout << "Z ROI: " << zStart_mm << " – " << zEnd_mm
              << " mm  (k " << kStart << " – " << kStart + nz_slice - 1 << ")" << std::endl;

    if (nx <= 0 || ny <= 0 || nz <= 0)
    {
        std::cerr << "Invalid voxel grid size. Check bounding box or voxel size." << std::endl;
        return 1;
    }

    // ---------------------------------------------------------------------
    // 3. Allocate ITK label image (unsigned short) for material IDs
    // ---------------------------------------------------------------------
    using LabelImageType = G4DatReader::LabelImageType;

    LabelImageType::Pointer image = LabelImageType::New();
    LabelImageType::SizeType size;
    size[0] = static_cast<LabelImageType::SizeType::SizeValueType>(nx_slice);
    size[1] = static_cast<LabelImageType::SizeType::SizeValueType>(ny_slice);
    size[2] = static_cast<LabelImageType::SizeType::SizeValueType>(nz_slice);

    LabelImageType::RegionType region;
    region.SetSize(size);

    image->SetRegions(region);
    image->Allocate();
    image->FillBuffer(0); // default to Air / background

    double spacing[3] = {voxelSize_mm, voxelSize_mm, voxelSize_mm};
    image->SetSpacing(spacing);

    double origin[3] = {iStart * voxelSize_mm, jStart * voxelSize_mm, kStart * voxelSize_mm};
    image->SetOrigin(origin);

    image->SetDirection(LabelImageType::DirectionType::GetIdentity());

    // ---------------------------------------------------------------------
    // 4. Voxelization loop using Geant4 G4Tet::Inside
    // ---------------------------------------------------------------------
    const G4int numTet = tetImport.GetNumTetrahedron();
    std::cout << "Starting voxelization over " << numTet << " tetrahedra..." << std::endl;

    for (G4int t = 0; t < numTet; ++t)
    {
        if (t % 100000 == 0)
        {
            std::cout << "Processing tet " << t << " / " << numTet << std::endl;
        }

        G4Tet *tetSolid = tetImport.GetTetrahedron(t);
        const std::vector<G4ThreeVector> &vertices = tetSolid->GetVertices();

        G4double tMinX(DBL_MAX), tMinY(DBL_MAX), tMinZ(DBL_MAX);
        G4double tMaxX(-DBL_MAX), tMaxY(-DBL_MAX), tMaxZ(-DBL_MAX);

        for (const auto &v : vertices)
        {
            if (v.x() < tMinX)
                tMinX = v.x();
            if (v.x() > tMaxX)
                tMaxX = v.x();
            if (v.y() < tMinY)
                tMinY = v.y();
            if (v.y() > tMaxY)
                tMaxY = v.y();
            if (v.z() < tMinZ)
                tMinZ = v.z();
            if (v.z() > tMaxZ)
                tMaxZ = v.z();
        }

        // Clamp tet bounding box to global phantom bounds
        tMinX = std::max(tMinX, bbMin.x());
        tMaxX = std::min(tMaxX, bbMax.x());
        tMinY = std::max(tMinY, bbMin.y());
        tMaxY = std::min(tMaxY, bbMax.y());
        tMinZ = std::max(tMinZ, bbMin.z());
        tMaxZ = std::min(tMaxZ, bbMax.z());

        const G4int iMin = static_cast<G4int>(std::floor((tMinX - bbMin.x()) / voxelSize));
        const G4int iMax = static_cast<G4int>(std::floor((tMaxX - bbMin.x()) / voxelSize));
        const G4int jMin = static_cast<G4int>(std::floor((tMinY - bbMin.y()) / voxelSize));
        const G4int jMax = static_cast<G4int>(std::floor((tMaxY - bbMin.y()) / voxelSize));
        const G4int kMin = static_cast<G4int>(std::floor((tMinZ - bbMin.z()) / voxelSize));
        const G4int kMax = static_cast<G4int>(std::floor((tMaxZ - bbMin.z()) / voxelSize));

        const G4int matID = tetImport.GetMaterialIndex(t);

        for (G4int k = std::max(kStart, kMin); k <= std::min(kMax, kStart + nz_slice - 1); ++k)
        {
            for (G4int j = std::max(jStart, jMin); j <= std::min(jMax, jStart + ny_slice - 1); ++j)
            {
                for (G4int i = std::max(iStart, iMin); i <= std::min(iMax, iStart + nx_slice - 1); ++i)
                {
                    const G4double x = bbMin.x() + (i + 0.5) * voxelSize;
                    const G4double y = bbMin.y() + (j + 0.5) * voxelSize;
                    const G4double z = bbMin.z() + (k + 0.5) * voxelSize;

                    G4ThreeVector pos(x, y, z);
                    if (tetSolid->Inside(pos) == kOutside)
                        continue;

                    LabelImageType::IndexType idx;
                    idx[0] = static_cast<LabelImageType::IndexType::IndexValueType>(i - iStart);
                    idx[1] = static_cast<LabelImageType::IndexType::IndexValueType>(j - jStart);
                    idx[2] = static_cast<LabelImageType::IndexType::IndexValueType>(k - kStart);

                    image->SetPixel(idx, static_cast<unsigned short>(matMap.getSeqIndex(matID)));
                }
            }
        }
    }

    std::cout << "Voxelization finished. Writing MCGPU RAW + info..." << std::endl;

    // ---------------------------------------------------------------------
    // 5. Save as MCGPU-compatible RAW + .txt using existing helper
    // ---------------------------------------------------------------------
    ImageUtils::LabelsToRawFile(image, outputBase);

    // ---------------------------------------------------------------------
    // 6. Write MC-GPU [SECTION MATERIAL FILE LIST] sidecar file
    // ---------------------------------------------------------------------
    matMap.writeMCGPUSectionFile(outputBase + "_mcgpu_section.txt", phantomName);

    // ---------------------------------------------------------------------
    // 7. Initialise Geant4 for μ computation (if not already running)
    // ---------------------------------------------------------------------
    G4RunManager *runManager = G4RunManager::GetRunManager();
    if (!runManager)
    {
        runManager = new G4RunManager();
        runManager->SetUserInitialization(new MiniDetector());
        runManager->SetUserInitialization(new FTFP_BERT());
        runManager->SetUserInitialization(new MiniAction());
        runManager->Initialize();
        runManager->BeamOn(1);
        std::cout << "Geant4 environment initialised for μ computation." << std::endl;
    }

    // ---------------------------------------------------------------------
    // 8. Build μ look-up table  [seq_index] → μ (cm⁻¹) at 60 keV
    // ---------------------------------------------------------------------
    const G4double energy60keV = 60.0 * keV;
    G4EmCalculator emCalc;
    G4NistManager *nist = G4NistManager::Instance();
    G4Material *water = nist->FindOrBuildMaterial("G4_WATER");
    const G4double muWater = 1.0 / (emCalc.ComputeGammaAttenuationLength(energy60keV, water) / cm);
    std::cout << "μ_water @ 60 keV = " << muWater << " cm^-1" << std::endl;

    const auto &entries = matMap.entries();
    std::vector<float> muLUT(entries.size(), 0.0f);
    for (const auto &e : entries)
    {
        G4Material *mat = (e.mrcp_id == 0)
                              ? nist->FindOrBuildMaterial("G4_AIR")
                              : tetImport.GetMaterial(e.mrcp_id);
        if (mat)
        {
            G4double mu = 1.0 / (emCalc.ComputeGammaAttenuationLength(energy60keV, mat) / cm);
            muLUT[e.seq_index] = static_cast<float>(mu);
        }
        else
        {
            std::cerr << "WARNING: no G4Material for MRCP ID " << e.mrcp_id
                      << " (seq " << e.seq_index << "), μ=0" << std::endl;
        }
    }

    // ---------------------------------------------------------------------
    // 9. Allocate μ float32 and HU int16 images; fill from label image
    // ---------------------------------------------------------------------
    using FloatImageType = itk::Image<float, 3>;
    using Int16ImageType = itk::Image<int16_t, 3>;

    auto muImage = FloatImageType::New();
    muImage->SetRegions(image->GetLargestPossibleRegion());
    muImage->SetSpacing(image->GetSpacing());
    muImage->SetOrigin(image->GetOrigin());
    muImage->SetDirection(image->GetDirection());
    muImage->Allocate();

    auto huImage = Int16ImageType::New();
    huImage->SetRegions(image->GetLargestPossibleRegion());
    huImage->SetSpacing(image->GetSpacing());
    huImage->SetOrigin(image->GetOrigin());
    huImage->SetDirection(image->GetDirection());
    huImage->Allocate();

    itk::ImageRegionConstIterator<LabelImageType> itLbl(image, image->GetLargestPossibleRegion());
    itk::ImageRegionIterator<FloatImageType> itMu(muImage, muImage->GetLargestPossibleRegion());
    itk::ImageRegionIterator<Int16ImageType> itHU(huImage, huImage->GetLargestPossibleRegion());

    for (itLbl.GoToBegin(), itMu.GoToBegin(), itHU.GoToBegin();
         !itLbl.IsAtEnd(); ++itLbl, ++itMu, ++itHU)
    {
        const unsigned short seqIdx = itLbl.Get();
        const float mu = (seqIdx < static_cast<unsigned short>(muLUT.size()))
                             ? muLUT[seqIdx]
                             : 0.0f;
        itMu.Set(mu);

        const double huVal = 1000.0 * (static_cast<double>(mu) - muWater) / muWater;
        const int16_t hu = static_cast<int16_t>(
            std::clamp(huVal,
                       static_cast<double>(std::numeric_limits<int16_t>::min()),
                       static_cast<double>(std::numeric_limits<int16_t>::max())));
        itHU.Set(hu);
    }

    std::cout << "μ and HU volumes populated." << std::endl;

    // ---------------------------------------------------------------------
    // 10. Write μ float32 raw
    // ---------------------------------------------------------------------

    std::string muPath = outputBase + "mu60keV_" +
                         std::to_string(nx_slice) + "x" +
                         std::to_string(ny_slice) + "x" +
                         std::to_string(nz_slice) + ".raw";
    std::ofstream muFile(muPath, std::ios::binary);
    itk::ImageRegionConstIterator<FloatImageType> it(muImage, muImage->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
        float v = it.Get();
        muFile.write(reinterpret_cast<const char *>(&v), sizeof(float));
    }
    muFile.close();
    std::cout << "μ float32 stack written to: " << muPath << std::endl;

    // ---------------------------------------------------------------------
    // 11. Write HU int16 raw
    // ---------------------------------------------------------------------

    std::string huPath = outputBase + "HU60keV_" +
                         std::to_string(nx_slice) + "x" +
                         std::to_string(ny_slice) + "x" +
                         std::to_string(nz_slice) + ".raw";
    std::ofstream huFile(huPath, std::ios::binary);
    itk::ImageRegionConstIterator<Int16ImageType> it2(huImage, huImage->GetLargestPossibleRegion());
    for (it2.GoToBegin(); !it2.IsAtEnd(); ++it2)
    {
        int16_t v = it2.Get();
        huFile.write(reinterpret_cast<const char *>(&v), sizeof(int16_t));
    }
    huFile.close();
    std::cout << "HU int16 stack @ 60 keV written to: " << huPath << std::endl;

    // ---------------------------------------------------------------------
    // 11b. Export HU volume as DICOM CT series
    // ---------------------------------------------------------------------
    {
        const std::string dicomDir = outputBase + "HU60keV_dicom_" +
                                     std::to_string(nx_slice) + "x" +
                                     std::to_string(ny_slice) + "x" +
                                     std::to_string(nz_slice);
        HUDicomExporter::Options dcmOpts;
        dcmOpts.patientName       = phantomName;
        dcmOpts.seriesDescription = phantomName + " HU @ 60 keV";
        HUDicomExporter::Write(huImage, dicomDir, dcmOpts);
    }

    // ---------------------------------------------------------------------
    // 12. Threshold HU → 5(+1)-label phantom
    //     0=air, 1=fat, 2=soft tissue, 3=bone spongiosa, 4=bone cortical
    //     5=implant (cylinder, optional)
    // ---------------------------------------------------------------------
    const int16_t THR_AIR_FAT        = thr_air_fat;
    const int16_t THR_FAT_SOFT       = thr_fat_soft;
    const int16_t THR_SOFT_SPONGIOSA = thr_soft_spongiosa;
    const int16_t THR_SPONGIOSA_CORT = thr_spongiosa_cort;

    const size_t totalVox = static_cast<size_t>(nx_slice) *
                            static_cast<size_t>(ny_slice) *
                            static_cast<size_t>(nz_slice);
    std::vector<uint8_t> labelBuf(totalVox);

    // -- HU thresholding --
    {
        itk::ImageRegionConstIterator<Int16ImageType> itHU2(huImage, huImage->GetLargestPossibleRegion());
        size_t idx = 0;
        for (itHU2.GoToBegin(); !itHU2.IsAtEnd(); ++itHU2, ++idx)
        {
            const int16_t hu = itHU2.Get();
            if      (hu < THR_AIR_FAT)        labelBuf[idx] = 0;
            else if (hu < THR_FAT_SOFT)       labelBuf[idx] = 1;
            else if (hu < THR_SOFT_SPONGIOSA) labelBuf[idx] = 2;
            else if (hu < THR_SPONGIOSA_CORT) labelBuf[idx] = 3;
            else                              labelBuf[idx] = 4;
        }
    }

    // -- Implant cylinder override (axis along Z) --
    const bool hasImplant = (implant_radius_mm > 0.0);
    if (hasImplant)
    {
        const double r2 = implant_radius_mm * implant_radius_mm;
        const double halfH = implant_height_mm / 2.0;
        size_t implantVoxels = 0;

        for (G4int k = 0; k < nz_slice; ++k)
        {
            // physical Z of voxel centre (mm from bbMin)
            const double vz = (kStart + k + 0.5) * voxelSize_mm;
            if (std::abs(vz - implant_cz_mm) > halfH) continue;

            for (G4int j = 0; j < ny_slice; ++j)
            {
                const double vy = (jStart + j + 0.5) * voxelSize_mm;
                const double dy = vy - implant_cy_mm;

                for (G4int i = 0; i < nx_slice; ++i)
                {
                    const double vx = (iStart + i + 0.5) * voxelSize_mm;
                    const double dx = vx - implant_cx_mm;

                    if (dx*dx + dy*dy <= r2)
                    {
                        const size_t flat = static_cast<size_t>(k) * ny_slice * nx_slice +
                                            static_cast<size_t>(j) * nx_slice +
                                            static_cast<size_t>(i);
                        labelBuf[flat] = 5;
                        ++implantVoxels;
                    }
                }
            }
        }
        std::cout << "Implant cylinder: centre=(" << implant_cx_mm << ", "
                  << implant_cy_mm << ", " << implant_cz_mm << ") mm"
                  << "  r=" << implant_radius_mm << " mm"
                  << "  h=" << implant_height_mm << " mm"
                  << "  → " << implantVoxels << " voxels labelled 5" << std::endl;
    }

    // -- Crop cylinder mask (zero everything outside) --
    if (crop_cylinder_enable)
    {
        // centre of the slice ROI in absolute voxel coords [mm]
        const double cx = crop_cylinder_cx_mm;
        const double cy = crop_cylinder_cy_mm;
        const double r2 = crop_cylinder_radius_mm * crop_cylinder_radius_mm;
        size_t zeroedVoxels = 0;

        for (G4int k = 0; k < nz_slice; ++k)
            for (G4int j = 0; j < ny_slice; ++j)
            {
                const double vy = (jStart + j + 0.5) * voxelSize_mm;
                const double dy = vy - cy;
                for (G4int i = 0; i < nx_slice; ++i)
                {
                    const double vx = (iStart + i + 0.5) * voxelSize_mm;
                    const double dx = vx - cx;
                    if (dx*dx + dy*dy > r2)
                    {
                        const size_t flat = static_cast<size_t>(k) * ny_slice * nx_slice +
                                            static_cast<size_t>(j) * nx_slice +
                                            static_cast<size_t>(i);
                        labelBuf[flat] = 0;
                        ++zeroedVoxels;
                    }
                }
            }
        std::cout << "Crop cylinder: centre=(" << cx << ", " << cy << ") mm"
                  << "  r=" << crop_cylinder_radius_mm << " mm"
                  << "  → " << zeroedVoxels << " voxels zeroed" << std::endl;
    }

    // -- Write raw --
    const std::string labelTag = std::string(hasImplant ? "5labels_implant_" : "5labels_")
                               + (crop_cylinder_enable ? "reconCylinder_" : "");
    std::string label4Path = outputBase + labelTag +
                             std::to_string(nx_slice) + "x" +
                             std::to_string(ny_slice) + "x" +
                             std::to_string(nz_slice) + ".raw";
    {
        std::ofstream label4File(label4Path, std::ios::binary);
        label4File.write(reinterpret_cast<const char*>(labelBuf.data()),
                         static_cast<std::streamsize>(totalVox));
    }
    std::cout << (hasImplant ? "6" : "5")
              << "-label phantom written to: " << label4Path << std::endl;

    // write companion info .txt (same geometry as the label phantom)
    {
        const std::string infoBase = outputBase + labelTag +
                                     std::to_string(nx_slice) + "x" +
                                     std::to_string(ny_slice) + "x" +
                                     std::to_string(nz_slice);
        const auto sp = image->GetSpacing();
        const auto sz = image->GetLargestPossibleRegion().GetSize();
        std::ofstream info4(infoBase + ".txt");
        info4 << std::fixed << std::setprecision(3);
        info4 << "#[SECTION VOXELIZED GEOMETRY FILE v.2017-07-26]\n";
        info4 << "phantom/" << std::filesystem::path(infoBase).filename().string()
              << ".raw     # VOXEL GEOMETRY FILE (penEasy 2008 format; .gz accepted)\n";
        info4 << " " << (-static_cast<double>(sz[0]) * sp[0] / 2.0 / 10.0)
              << "  " << (-static_cast<double>(sz[1]) * sp[1] / 2.0 / 10.0)
              << "  " << (-static_cast<double>(sz[2]) * sp[2] / 2.0 / 10.0)
              << "              # OFFSET OF THE VOXEL GEOMETRY [cm]\n";
        info4 << " " << sz[0] << " " << sz[1] << " " << sz[2]
              << "                 # NUMBER OF VOXELS\n";
        info4 << " " << (sp[0] / 10.0) << " " << (sp[1] / 10.0) << " " << (sp[2] / 10.0)
              << "           # VOXEL SIZES [cm]\n";
        info4 << " 0 0 0                          # SIZE OF LOW RESOLUTION VOXELS\n";
        std::cout << (hasImplant ? "6" : "5") << "-label info file written: " << infoBase << ".txt" << std::endl;
    }

    std::cout << "Done." << std::endl;
    return 0;
}
