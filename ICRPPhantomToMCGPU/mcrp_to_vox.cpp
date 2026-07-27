#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                s = s.substr(1, s.size() - 2);
            return s;
        };
        cfg[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return cfg;
}

// ---------------------------------------------------------------------------
// Extract the first `count` non-comment "value" line contents after the first
// line containing `marker`.  Mirrors expand_in_kv.py's _in_section_values, so
// this reads the same MC-GPU .in section layout the positioning-check script
// already parses (SOURCE / IMAGE DETECTOR).
// ---------------------------------------------------------------------------
static std::vector<std::string> inSectionValues(const std::vector<std::string>& lines,
                                                  const std::string& marker, size_t count)
{
    std::vector<std::string> vals;
    bool grabbing = false;
    for (const auto& raw : lines) {
        if (!grabbing) {
            if (raw.find(marker) != std::string::npos) grabbing = true;
            continue;
        }
        std::string body = raw.substr(0, raw.find('#'));
        size_t a = body.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) continue;   // blank / fully-commented line
        size_t b = body.find_last_not_of(" \t\r\n");
        vals.push_back(body.substr(a, b - a + 1));
        if (vals.size() >= count) break;
    }
    return vals;
}

// ---------------------------------------------------------------------------
// Parse an MC-GPU .in template's beam geometry and return the half-height (in
// mm, isocenter-relative) of the reconstructed field of view along Z (the
// cranial-caudal axis), i.e. how far above/below isocenter the fan/cone beam
// still reaches the detector.  Returns -1.0 if the template can't be read or
// parsed (SOURCE / IMAGE DETECTOR sections missing).
//   FOV_halfZ = (detector_height/2) * (SAD / SDD)      [similar triangles]
// ---------------------------------------------------------------------------
static double parseBeamFOVHalfZ_mm(const std::string& tmplPath)
{
    std::ifstream f(tmplPath);
    if (!f) return -1.0;
    std::vector<std::string> lines;
    { std::string l; while (std::getline(f, l)) lines.push_back(l); }

    // SECTION SOURCE: value[0] = active spectrum line, value[1] = source position
    auto src = inSectionValues(lines, "SECTION SOURCE", 2);
    // SECTION IMAGE DETECTOR: [0]=output name [1]=pixels [2]=image size [3]=SDD
    auto det = inSectionValues(lines, "SECTION IMAGE DETECTOR", 4);
    if (src.size() < 2 || det.size() < 4) return -1.0;

    double sx, sy, sz, dx, dz, sdd;
    std::istringstream ssPos(src[1]);
    std::istringstream ssImg(det[2]);
    std::istringstream ssSdd(det[3]);
    if (!(ssPos >> sx >> sy >> sz)) return -1.0;
    if (!(ssImg >> dx >> dz))       return -1.0;
    if (!(ssSdd >> sdd))            return -1.0;

    const double sad_cm = std::abs(sy);   // source-to-isocenter distance [cm]
    if (sdd <= 0.0 || sad_cm <= 0.0) return -1.0;

    const double fovHalfZ_cm = (dz / 2.0) * (sad_cm / sdd);
    return fovHalfZ_cm * 10.0;   // -> mm
}

struct ImplantCylinder {
    double cx_mm, cy_mm, cz_mm, radius_mm, height_mm;
};

// ---------------------------------------------------------------------------
// Stretcher shell (coords in isocenter mm) — ported from dicom_to_mcgpu.cpp.
// Rounded polygon cross-section: a carbon wall (label 10) of fixed thickness
// around a foam core (label 11), extending over the full Z range.
// ---------------------------------------------------------------------------
struct StretcherShell {
    // Isocenter coordinates (mm) of the stretcher cross-section polygon centre.
    double cx_mm = 0.0;  // lateral X of the polygon centre, from isocenter [mm]
    double cy_mm = 0.0;  // vertical Y of the polygon centre, from isocenter [mm]
};

// 2-D convex polygon helpers
using Poly2D = std::vector<std::array<double, 2>>;

static bool inConvex(const Poly2D& poly, double x, double y)
{
    const size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        double ax = poly[i][0],        ay = poly[i][1];
        double bx = poly[(i+1)%n][0],  by = poly[(i+1)%n][1];
        double ex = bx-ax, ey = by-ay;
        if ((x-ax)*(-ey) + (y-ay)*ex < 0.0) return false;
    }
    return true;
}

static Poly2D inwardOffset(const Poly2D& poly, double d)
{
    const size_t n = poly.size();
    std::vector<std::array<double,2>> norms(n);
    for (size_t i = 0; i < n; ++i) {
        double dx = poly[(i+1)%n][0]-poly[i][0];
        double dy = poly[(i+1)%n][1]-poly[i][1];
        double L  = std::sqrt(dx*dx+dy*dy);
        norms[i]  = {-dy/L, dx/L};
    }
    Poly2D inner(n);
    for (size_t i = 0; i < n; ++i) {
        size_t prev = (i+n-1)%n;
        double p1x = poly[prev][0]+d*norms[prev][0], p1y = poly[prev][1]+d*norms[prev][1];
        double d1x = poly[i][0]-poly[prev][0],        d1y = poly[i][1]-poly[prev][1];
        double p2x = poly[i][0]+d*norms[i][0],        p2y = poly[i][1]+d*norms[i][1];
        double d2x = poly[(i+1)%n][0]-poly[i][0],     d2y = poly[(i+1)%n][1]-poly[i][1];
        double det = d1x*(-d2y)-(-d2x)*d1y;
        if (std::abs(det) < 1e-10) { inner[i] = {p2x, p2y}; continue; }
        double bx_ = p2x-p1x, by_ = p2y-p1y;
        double t   = (bx_*(-d2y)-by_*(-d2x))/det;
        inner[i]   = {p1x+t*d1x, p1y+t*d1y};
    }
    return inner;
}

// ---------------------------------------------------------------------------
// Rounded-corner polygon (mirrors stretcher_profile.py round_polygon).
// Replaces each vertex of a convex CCW polygon with a circular arc of radius r.
// n_arc points per corner.  r = 0 → returns the original polygon unchanged.
// ---------------------------------------------------------------------------
static Poly2D roundPolygon(const Poly2D& poly, double r, int n_arc = 20)
{
    if (r < 1e-9) return poly;

    static const double kTwoPi = 2.0 * std::acos(-1.0);
    const size_t n = poly.size();
    Poly2D result;
    result.reserve(n * static_cast<size_t>(n_arc));

    for (size_t i = 0; i < n; ++i) {
        const auto& p0 = poly[(i + n - 1) % n];
        const auto& p1 = poly[i];
        const auto& p2 = poly[(i + 1) % n];

        double dx_in  = p1[0]-p0[0], dy_in  = p1[1]-p0[1];
        double L_in   = std::sqrt(dx_in*dx_in + dy_in*dy_in);
        double ux_in  = dx_in/L_in,  uy_in  = dy_in/L_in;

        double dx_out = p2[0]-p1[0], dy_out = p2[1]-p1[1];
        double L_out  = std::sqrt(dx_out*dx_out + dy_out*dy_out);
        double ux_out = dx_out/L_out, uy_out = dy_out/L_out;

        double cos_a   = std::clamp(ux_in*ux_out + uy_in*uy_out, -1.0, 1.0);
        double half_ext = std::acos(cos_a) / 2.0;

        if (half_ext < 1e-9) { result.push_back(p1); continue; }

        double d    = r * std::tan(half_ext);
        double tp1x = p1[0] - d*ux_in,  tp1y = p1[1] - d*uy_in;
        double tp2x = p1[0] + d*ux_out, tp2y = p1[1] + d*uy_out;

        // Left normal of u_in (inward for CCW); flip if the turn is right-handed
        double px = -uy_in, py = ux_in;
        if (ux_in*uy_out - uy_in*ux_out < 0) { px = -px; py = -py; }

        double cx = tp1x + r*px, cy_c = tp1y + r*py;

        double a1 = std::atan2(tp1y - cy_c, tp1x - cx);
        double a2 = std::atan2(tp2y - cy_c, tp2x - cx);
        if (a2 < a1) a2 += kTwoPi;

        double r_arc = std::sqrt((tp1x-cx)*(tp1x-cx) + (tp1y-cy_c)*(tp1y-cy_c));
        for (int j = 0; j < n_arc; ++j) {
            double t = a1 + (a2 - a1) * j / (n_arc - 1);
            result.push_back({ cx + r_arc*std::cos(t), cy_c + r_arc*std::sin(t) });
        }
    }
    return result;
}

int main(int argc, char **argv)
{
    // ---------------------------------------------------------------------
    // 1. Parse parameters — from a .cfg file or positional args
    // ---------------------------------------------------------------------

    // defaults
    std::string phantomName   = "MRCP_AF";
    // Base folder every output subfolder is written under (both legacy ROI
    // mode and fixed-volume mode).  May be relative (to the CWD the binary is
    // run from) or absolute; use to redirect output away from ./output, e.g.
    // onto a data drive.  Trailing slash optional.
    std::string output_dir    = "./output";
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
    double crop_cylinder_cx_mm     = 0.0;   // isocenter-relative [mm] (as dicom_to_mcgpu)
    double crop_cylinder_cy_mm     = 0.0;
    // implant cylinders (axis along Z); populated from cfg or positional args
    std::vector<ImplantCylinder> implants;
    // stretcher
    bool           stretcher_enable = false;
    StretcherShell stretcher;
    // geometry fine-offset [mm]; shift_x/y also accumulate the extra shift
    // when the volume is padded to fit the stretcher
    double shift_x_mm = 0.0;
    double shift_y_mm = 0.0;
    double shift_z_mm = 0.0;
    // Fixed output-volume mode (production; mirrors dicom_to_mcgpu). Active when
    // vol_width/height/length are all > 0: the phantom is placed inside a fixed
    // air volume (isocenter = volume centre) instead of the ROI being clamped to
    // the phantom bbox. Lean path — only the 5-label .raw + .txt + .in are made.
    double vol_vxy_mm    = 0.0;   // 0 -> fall back to voxel_size_mm
    double vol_vz_mm     = 0.0;
    double vol_width_mm  = 0.0;   // X extent [mm]
    double vol_height_mm = 0.0;   // Y extent [mm]
    double vol_length_mm = 0.0;   // Z extent [mm]
    double head_region_mm = 100.0;   // top slab used to centre XY + seat stretcher
    bool   stretcher_auto = true;     // auto-seat the shell behind the head (+Y)
    double stretcher_gap_mm = 2.0;    // gap between head posterior surface and shell
    bool   write_dicom = false;       // fixed mode: also build int16 HU raw + DICOM
    // Air gap kept between the head tip and the top of the reconstructed CBCT
    // field of view (derived from the .in template's beam geometry — SAD, SDD,
    // detector height).  Keeps the FOV centred on the head instead of clipping
    // its top, while leaving a bit of headroom.  Falls back to the same margin
    // below the volume's own top face if no template geometry is parseable.
    double head_top_margin_mm = 10.0;
    // TIGHT-CROP DICOM EXPORT MODE (lean; no MC-GPU output at all — see
    // HANDOUT_mcrp_dicom_stretcher_augmentation.md).  Exports ONLY a DICOM HU
    // series, sized to the actual captured anatomy (head tip down to
    // vol_length_mm, clamped to the phantom's own extent if shorter) plus a
    // flat air margin on every side, instead of a large fixed volume or the
    // beam-FOV-anchored placement above.  No labels, no overlays, no .in/.raw.
    bool   tight_crop_dicom = false;
    double air_margin_mm    = 20.0;
    // MC-GPU .in template
    std::string mcgpu_in_template;
    std::string mcgpu_output_name;
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
        phantomName      = getS("phantom_name",      phantomName);
        output_dir       = getS("output_dir",        output_dir);
        voxelSize_mm     = getD("voxel_size_mm",     voxelSize_mm);
        vol_vxy_mm       = getD("vol_vxy_mm",        vol_vxy_mm);
        vol_vz_mm        = getD("vol_vz_mm",         vol_vz_mm);
        vol_width_mm     = getD("vol_width_mm",      vol_width_mm);
        vol_height_mm    = getD("vol_height_mm",     vol_height_mm);
        vol_length_mm    = getD("vol_length_mm",     vol_length_mm);
        head_region_mm   = getD("head_region_mm",    head_region_mm);
        stretcher_gap_mm = getD("stretcher_gap_mm",  stretcher_gap_mm);
        head_top_margin_mm = getD("head_top_margin_mm", head_top_margin_mm);
        write_dicom      = cfg.count("write_dicom")
            ? (cfg["write_dicom"] == "1" || cfg["write_dicom"] == "true") : write_dicom;
        tight_crop_dicom = cfg.count("tight_crop_dicom")
            ? (cfg["tight_crop_dicom"] == "1" || cfg["tight_crop_dicom"] == "true") : tight_crop_dicom;
        air_margin_mm    = getD("air_margin_mm",     air_margin_mm);
        xStart_mm        = getD("x_start_mm",        xStart_mm);
        xEnd_mm          = getD("x_end_mm",          xEnd_mm);
        yStart_mm        = getD("y_start_mm",        yStart_mm);
        yEnd_mm          = getD("y_end_mm",          yEnd_mm);
        zStart_mm        = getD("z_start_mm",        zStart_mm);
        zEnd_mm          = getD("z_end_mm",          zEnd_mm);
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
            // backward-compat: single implant via old-style keys
            const double r = getD("implant_radius_mm", -1.0);
            if (r > 0.0)
            {
                ImplantCylinder imp;
                imp.cx_mm     = getD("implant_cx_mm",    275.5);
                imp.cy_mm     = getD("implant_cy_mm",     74.5);
                imp.cz_mm     = getD("implant_cz_mm",   1462.0);
                imp.radius_mm = r;
                imp.height_mm = getD("implant_height_mm", 22.0);
                implants.push_back(imp);
            }
        }
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
        stretcher_enable = cfg.count("stretcher_enable")
            ? (cfg["stretcher_enable"] == "1" || cfg["stretcher_enable"] == "true")
            : stretcher_enable;
        stretcher.cx_mm = getD("stretcher_cx_mm", stretcher.cx_mm);
        stretcher.cy_mm = getD("stretcher_cy_mm", stretcher.cy_mm);
        stretcher_auto  = cfg.count("stretcher_auto")
            ? (cfg["stretcher_auto"] == "1" || cfg["stretcher_auto"] == "true")
            : stretcher_auto;
        shift_x_mm = getD("shift_x_mm", shift_x_mm);
        shift_y_mm = getD("shift_y_mm", shift_y_mm);
        shift_z_mm = getD("shift_z_mm", shift_z_mm);
        mcgpu_in_template  = getS("mcgpu_in_template",  mcgpu_in_template);
        mcgpu_output_name  = getS("mcgpu_output_name",  mcgpu_output_name);
        mcgpu_det_nx       = static_cast<int>(getD("mcgpu_det_nx", mcgpu_det_nx));
        mcgpu_det_nz       = static_cast<int>(getD("mcgpu_det_nz", mcgpu_det_nz));
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
        if (argc > 9)
        {
            ImplantCylinder imp;
            imp.cx_mm     = std::stod(argv[9]);
            imp.cy_mm     = (argc > 10) ? std::stod(argv[10]) : 0.0;
            imp.cz_mm     = (argc > 11) ? std::stod(argv[11]) : 0.0;
            imp.radius_mm = (argc > 12) ? std::stod(argv[12]) : 0.0;
            imp.height_mm = (argc > 13) ? std::stod(argv[13]) : 0.0;
            if (imp.radius_mm > 0.0)
                implants.push_back(imp);
        }
    }

    // Strip any trailing slash so "<output_dir>/" concatenation never doubles up.
    while (!output_dir.empty() && (output_dir.back() == '/' || output_dir.back() == '\\'))
        output_dir.pop_back();
    if (output_dir.empty()) output_dir = ".";
    std::filesystem::create_directories(output_dir);
    std::cout << "Output base dir: " << output_dir << std::endl;

    // outputPrefix: cfg stem when using a cfg file, otherwise phantom name
    const std::string outputPrefix = usingCfg
        ? std::filesystem::path(argv[1]).stem().string()
        : phantomName;
    std::string outputBase = output_dir + "/" + outputPrefix + "_vox_";

    std::cout << "Using phantom: " << phantomName << std::endl;
    std::cout << "Target voxel size: " << voxelSize_mm << " mm" << std::endl;

    // ---------------------------------------------------------------------
    // 1b. Load material map (MRCP IDs → sequential uint8 indices)
    // ---------------------------------------------------------------------
    MCRPMaterialMap matMap;
    {
        const std::string mapDir = "./data/mcgpu_mcrp_materials/" + phantomName;
        if (!std::filesystem::exists(mapDir + "/MC-GPU_material_config.txt"))
        {
            std::cerr << "ERROR: no material map for phantom '" << phantomName << "'\n"
                      << "Expected: " << mapDir << "/MC-GPU_material_config.txt\n"
                      << "(check the exact name — paediatric phantoms use a dash, e.g. MRCP-00F)\n"
                      << "Available phantoms:\n";
            for (const auto& e : std::filesystem::directory_iterator("./data/mcgpu_mcrp_materials"))
                if (e.is_directory())
                    std::cerr << "  " << e.path().filename().string() << "\n";
            return 1;
        }
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

    // =====================================================================
    // TIGHT-CROP DICOM EXPORT MODE (lean; no MC-GPU output at all).
    // Exports ONLY a DICOM HU series, sized to the actual captured anatomy —
    // head tip down to vol_length_mm (clamped to the phantom's own extent if
    // shorter, e.g. a newborn) — plus a flat air_margin_mm on every side,
    // instead of a big fixed volume.  No labels, no implant/crop/stretcher
    // overlays, no .in/.raw/.txt.  Downstream placement, stretcher overlay,
    // and the final fixed-size MC-GPU volume are dicom_to_mcgpu's job — this
    // DICOM is fed into it as if it were a real patient series.  See
    // HANDOUT_mcrp_dicom_stretcher_augmentation.md for the full plan.
    // =====================================================================
    if (tight_crop_dicom)
    {
        const double out_vxy = (vol_vxy_mm > 0.0) ? vol_vxy_mm : voxelSize_mm;
        const double out_vz  = (vol_vz_mm  > 0.0) ? vol_vz_mm  : voxelSize_mm;

        // Captured Z window: head tip downward by vol_length_mm, clamped to
        // the phantom's own extent if it is shorter than that.
        const double zWindowTop    = bbMax.z();
        const double zWindowBottom = (vol_length_mm > 0.0)
            ? std::max(bbMin.z(), bbMax.z() - vol_length_mm * mm)
            : bbMin.z();

        // Tight XY bbox of every tetrahedron with any vertex inside the
        // captured Z window (not just the head) — so nothing wider than the
        // head (e.g. shoulders) gets cropped away.
        const G4int numTet = tetImport.GetNumTetrahedron();
        double hx0=DBL_MAX, hx1=-DBL_MAX, hy0=DBL_MAX, hy1=-DBL_MAX;
        for (G4int t = 0; t < numTet; ++t) {
            const auto& vs = tetImport.GetTetrahedron(t)->GetVertices();
            for (const auto& v : vs)
                if (v.z() >= zWindowBottom && v.z() <= zWindowTop) {
                    hx0 = std::min(hx0, v.x()); hx1 = std::max(hx1, v.x());
                    hy0 = std::min(hy0, v.y()); hy1 = std::max(hy1, v.y());
                }
        }
        if (hx0 > hx1) { hx0=bbMin.x(); hx1=bbMax.x(); hy0=bbMin.y(); hy1=bbMax.y(); }

        const double margin_g = air_margin_mm * mm;
        const double lenX_g = (hx1-hx0) + 2.0*margin_g;
        const double lenY_g = (hy1-hy0) + 2.0*margin_g;
        const double lenZ_g = (zWindowTop-zWindowBottom) + margin_g;   // margin only above the head tip

        const double out_vxy_g = out_vxy * mm, out_vz_g = out_vz * mm;
        const G4int out_nx = static_cast<G4int>(std::ceil(lenX_g/out_vxy_g));
        const G4int out_ny = static_cast<G4int>(std::ceil(lenY_g/out_vxy_g));
        const G4int out_nz = static_cast<G4int>(std::ceil(lenZ_g/out_vz_g));
        const size_t slicePx  = static_cast<size_t>(out_nx) * out_ny;
        const size_t totalVox = slicePx * out_nz;

        const double halfX = out_nx * out_vxy_g * 0.5;
        const double halfY = out_ny * out_vxy_g * 0.5;
        const double halfZ = out_nz * out_vz_g  * 0.5;
        const double isoPx = 0.5 * (hx0 + hx1);
        const double isoPy = 0.5 * (hy0 + hy1);
        const double isoPz = zWindowTop + margin_g - halfZ;   // head tip sits margin_g below the top face

        std::cout << "[tight-crop DICOM export mode]\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Captured Z window: [" << zWindowBottom/mm << ", " << zWindowTop/mm
                  << "] mm  (" << (zWindowTop-zWindowBottom)/mm << " mm)";
        if (vol_length_mm > 0.0 && (zWindowTop-zWindowBottom) < vol_length_mm*mm - 1e-6)
            std::cout << "  [phantom shorter than requested vol_length_mm="
                      << vol_length_mm << " mm; using its full extent]";
        std::cout << "\n";
        std::cout << "Tight XY bbox     : x[" << hx0/mm << ", " << hx1/mm << "]  y["
                  << hy0/mm << ", " << hy1/mm << "] mm  + " << air_margin_mm << " mm margin\n";
        std::cout << "Output volume     : " << out_nx << " x " << out_ny << " x " << out_nz
                  << "  (" << lenX_g/mm << " x " << lenY_g/mm << " x " << lenZ_g/mm << " mm)\n";
        std::cout << "Output spacing    : " << out_vxy << " x " << out_vxy << " x " << out_vz << " mm\n";
        std::cout << "HU volume         : "
                  << (totalVox*sizeof(int16_t)/(1024.0*1024.0*1024.0)) << " GiB\n" << std::flush;

        // ---- Geant4 + per-material HU LUT (mu@60keV -> HU); no label thresholds needed ----
        G4RunManager* runManager = G4RunManager::GetRunManager();
        if (!runManager) {
            runManager = new G4RunManager();
            runManager->SetUserInitialization(new MiniDetector());
            runManager->SetUserInitialization(new FTFP_BERT());
            runManager->SetUserInitialization(new MiniAction());
            runManager->Initialize();
            runManager->BeamOn(1);
        }
        const G4double energy60keV = 60.0 * keV;
        G4EmCalculator emCalc;
        G4NistManager* nist = G4NistManager::Instance();
        G4Material* water = nist->FindOrBuildMaterial("G4_WATER");
        const G4double muWater =
            1.0 / (emCalc.ComputeGammaAttenuationLength(energy60keV, water) / cm);

        const auto& entries = matMap.entries();
        std::vector<int16_t> huLUT(entries.size(), int16_t(-1000));
        for (const auto& e : entries) {
            G4Material* mat = (e.mrcp_id == 0)
                ? nist->FindOrBuildMaterial("G4_AIR")
                : tetImport.GetMaterial(e.mrcp_id);
            if (!mat) continue;
            const G4double mu =
                1.0 / (emCalc.ComputeGammaAttenuationLength(energy60keV, mat) / cm);
            const double hu = 1000.0 * (mu - muWater) / muWater;
            huLUT[e.seq_index] = static_cast<int16_t>(std::clamp(hu, -32768.0, 32767.0));
        }

        // ---- Allocate HU volume (air background), voxelize ----
        HUDicomExporter::HUImageType::Pointer huImage = HUDicomExporter::HUImageType::New();
        HUDicomExporter::HUImageType::SizeType sz;
        sz[0]=out_nx; sz[1]=out_ny; sz[2]=out_nz;
        HUDicomExporter::HUImageType::RegionType reg; reg.SetSize(sz);
        huImage->SetRegions(reg); huImage->Allocate();
        const double spc[3] = {out_vxy, out_vxy, out_vz};
        huImage->SetSpacing(spc);
        const double org[3] = {-out_nx*out_vxy/2.0, -out_ny*out_vxy/2.0, -out_nz*out_vz/2.0};
        huImage->SetOrigin(org);
        int16_t* hp = huImage->GetBufferPointer();
        std::fill(hp, hp + totalVox, int16_t(-1000));

        std::cout << "Voxelizing " << numTet << " tetrahedra...\n" << std::flush;
        for (G4int t = 0; t < numTet; ++t) {
            if (t % 500000 == 0) std::cout << "  tet " << t << " / " << numTet << "\n" << std::flush;

            G4Tet* tet = tetImport.GetTetrahedron(t);
            const auto& vtx = tet->GetVertices();
            double xm=DBL_MAX,xM=-DBL_MAX,ym=DBL_MAX,yM=-DBL_MAX,zm=DBL_MAX,zM=-DBL_MAX;
            for (const auto& v : vtx) {
                xm=std::min(xm,v.x()); xM=std::max(xM,v.x());
                ym=std::min(ym,v.y()); yM=std::max(yM,v.y());
                zm=std::min(zm,v.z()); zM=std::max(zM,v.z());
            }
            if (zM < zWindowBottom || zm > zWindowTop) continue;   // outside the captured window

            const int16_t hu = huLUT[matMap.getSeqIndex(tetImport.GetMaterialIndex(t))];

            auto lo = [](double c,double iso,double half,double sp,G4int){
                return std::max(0, (G4int)std::floor((c-iso+half)/sp - 0.5)); };
            auto hi = [](double c,double iso,double half,double sp,G4int n){
                return std::min(n-1, (G4int)std::ceil((c-iso+half)/sp - 0.5)); };
            const G4int i0=lo(xm,isoPx,halfX,out_vxy_g,out_nx), i1=hi(xM,isoPx,halfX,out_vxy_g,out_nx);
            const G4int j0=lo(ym,isoPy,halfY,out_vxy_g,out_ny), j1=hi(yM,isoPy,halfY,out_vxy_g,out_ny);
            const G4int k0=lo(zm,isoPz,halfZ,out_vz_g, out_nz), k1=hi(zM,isoPz,halfZ,out_vz_g, out_nz);
            for (G4int k=k0;k<=k1;++k) {
                const double pz=(k+0.5)*out_vz_g -halfZ+isoPz;
                for (G4int j=j0;j<=j1;++j) {
                    const double py=(j+0.5)*out_vxy_g-halfY+isoPy;
                    for (G4int i=i0;i<=i1;++i) {
                        const double px=(i+0.5)*out_vxy_g-halfX+isoPx;
                        if (tet->Inside(G4ThreeVector(px,py,pz))==kOutside) continue;
                        hp[(size_t)k*slicePx + (size_t)j*out_nx + i] = hu;
                    }
                }
            }
        }
        std::cout << "Voxelization done.\n";

        const std::string dimTag = std::to_string(out_nx)+"x"+std::to_string(out_ny)+"x"+std::to_string(out_nz);
        const std::string dicomDir = output_dir + "/" + phantomName + "_dicom_" + dimTag;
        HUDicomExporter::Options opt;
        opt.patientName       = phantomName;
        opt.seriesDescription = phantomName + " MRCP tight-crop HU @ 60 keV";
        HUDicomExporter::Write(huImage, dicomDir, opt);
        std::cout << "DICOM export written to: " << dicomDir << "\n";
        std::cout << "Done.\n";
        return 0;
    }

    // =====================================================================
    // FIXED OUTPUT-VOLUME MODE (production; mirrors dicom_to_mcgpu.cpp).
    // Active when vol_width/height/length are all > 0.  The phantom is placed
    // inside a fixed air volume (isocenter = volume centre); lean path writes
    // only the uint8 5-label .raw + .txt + .in (no full mu/HU/DICOM volumes).
    // Overlays (implant / crop / stretcher) use isocenter mm coords, identical
    // to dicom_to_mcgpu — no padding, the volume is already large enough.
    // =====================================================================
    if (vol_width_mm > 0.0 && vol_height_mm > 0.0 && vol_length_mm > 0.0)
    {
        const double out_vxy = (vol_vxy_mm > 0.0) ? vol_vxy_mm : voxelSize_mm;
        const double out_vz  = (vol_vz_mm  > 0.0) ? vol_vz_mm  : voxelSize_mm;
        const G4int out_nx = static_cast<G4int>(std::ceil(vol_width_mm  / out_vxy));
        const G4int out_ny = static_cast<G4int>(std::ceil(vol_height_mm / out_vxy));
        const G4int out_nz = static_cast<G4int>(std::ceil(vol_length_mm / out_vz));
        const size_t slicePx  = static_cast<size_t>(out_nx) * out_ny;
        const size_t totalVox = slicePx * out_nz;

        std::cout << "[fixed-volume mode]\n";
        std::cout << "Output volume : " << out_nx << " x " << out_ny << " x " << out_nz
                  << "  (" << vol_width_mm << " x " << vol_height_mm << " x "
                  << vol_length_mm << " mm)\n";
        std::cout << "Output spacing: " << out_vxy << " x " << out_vxy << " x "
                  << out_vz << " mm\n";
        std::cout << "Label buffer  : "
                  << (totalVox / (1024.0*1024.0*1024.0)) << " GiB\n" << std::flush;

        // ---- Placement pre-pass: XY bbox of the top head_region_mm slab ----
        const G4int numTet = tetImport.GetNumTetrahedron();
        const double headBandZmin = bbMax.z() - head_region_mm * mm;
        double hx0 = DBL_MAX, hx1 = -DBL_MAX, hy0 = DBL_MAX, hy1 = -DBL_MAX;
        for (G4int t = 0; t < numTet; ++t) {
            const auto& vs = tetImport.GetTetrahedron(t)->GetVertices();
            for (const auto& v : vs)
                if (v.z() >= headBandZmin) {
                    hx0 = std::min(hx0, v.x()); hx1 = std::max(hx1, v.x());
                    hy0 = std::min(hy0, v.y()); hy1 = std::max(hy1, v.y());
                }
        }
        if (hx0 > hx1) {   // fallback: whole phantom (head_region larger than phantom)
            hx0 = bbMin.x(); hx1 = bbMax.x(); hy0 = bbMin.y(); hy1 = bbMax.y();
        }

        // Phantom point placed at the isocenter (= volume centre), G4 units:
        //   XY: centre of the head slab (head sits at isocenter, like a head CBCT)
        //   Z : head tip (bbMax.z) anchored near the TOP OF THE BEAM'S FOV, not
        //       the volume's own top face — otherwise the head sits mostly above
        //       the reconstructed field of view and only its lower part is ever
        //       inside the beam.  FOV half-height is derived from the first
        //       mcgpu_in_template's beam geometry (SAD / SDD / detector height);
        //       falls back to a margin below the volume's top face if no
        //       template geometry is available.
        const double out_vxy_g = out_vxy * mm;
        const double out_vz_g  = out_vz  * mm;
        const double halfX = out_nx * out_vxy_g * 0.5;
        const double halfY = out_ny * out_vxy_g * 0.5;
        const double halfZ = out_nz * out_vz_g  * 0.5;
        const double isoPx = 0.5 * (hx0 + hx1);
        const double isoPy = 0.5 * (hy0 + hy1);

        double headTipTargetZ_mm = halfZ / mm - head_top_margin_mm;   // fallback
        if (!mcgpu_in_template.empty()) {
            std::string firstTmpl = mcgpu_in_template;
            const auto sep = firstTmpl.find_first_of(",;");
            if (sep != std::string::npos) firstTmpl = firstTmpl.substr(0, sep);
            firstTmpl.erase(0, firstTmpl.find_first_not_of(" \t\r\n"));
            firstTmpl.erase(firstTmpl.find_last_not_of(" \t\r\n") + 1);
            const double fovHalfZ_mm = parseBeamFOVHalfZ_mm(firstTmpl);
            if (fovHalfZ_mm > 0.0) {
                headTipTargetZ_mm = std::min(headTipTargetZ_mm, fovHalfZ_mm - head_top_margin_mm);
                std::cout << "Beam FOV      : half-height " << fovHalfZ_mm
                          << " mm (from " << firstTmpl << ")\n";
            } else {
                std::cout << "Warning: could not parse beam geometry from "
                          << firstTmpl << "; head tip placed near volume top face.\n";
            }
        }
        const double isoPz = bbMax.z() - headTipTargetZ_mm * mm;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Head slab     : x[" << hx0/mm << ", " << hx1/mm << "]  y["
                  << hy0/mm << ", " << hy1/mm << "] mm  (top " << head_region_mm << " mm)\n";
        std::cout << "Head AP depth : " << (hy1 - hy0)/mm << " mm  ->  head centred at isocenter\n";
        std::cout << "Z placement   : head tip " << bbMax.z()/mm
                  << " mm -> isocenter Z=" << headTipTargetZ_mm
                  << " mm (margin " << head_top_margin_mm
                  << " mm below FOV/volume top); volume spans "
                  << vol_length_mm << " mm downward\n";

        // ---- Geant4 + per-material label LUT (mu@60keV -> HU -> threshold) ----
        G4RunManager* runManager = G4RunManager::GetRunManager();
        if (!runManager) {
            runManager = new G4RunManager();
            runManager->SetUserInitialization(new MiniDetector());
            runManager->SetUserInitialization(new FTFP_BERT());
            runManager->SetUserInitialization(new MiniAction());
            runManager->Initialize();
            runManager->BeamOn(1);
        }
        const G4double energy60keV = 60.0 * keV;
        G4EmCalculator emCalc;
        G4NistManager* nist = G4NistManager::Instance();
        G4Material* water = nist->FindOrBuildMaterial("G4_WATER");
        const G4double muWater =
            1.0 / (emCalc.ComputeGammaAttenuationLength(energy60keV, water) / cm);

        const auto& entries = matMap.entries();
        std::vector<uint8_t> labelLUT(entries.size(), 0);
        std::vector<int16_t> huLUT(entries.size(), int16_t(-1000));
        for (const auto& e : entries) {
            G4Material* mat = (e.mrcp_id == 0)
                ? nist->FindOrBuildMaterial("G4_AIR")
                : tetImport.GetMaterial(e.mrcp_id);
            if (!mat) continue;
            const G4double mu =
                1.0 / (emCalc.ComputeGammaAttenuationLength(energy60keV, mat) / cm);
            const double hu = 1000.0 * (mu - muWater) / muWater;
            huLUT[e.seq_index] = static_cast<int16_t>(std::clamp(hu, -32768.0, 32767.0));
            uint8_t lbl;
            if      (hu < thr_air_fat)        lbl = 0;
            else if (hu < thr_fat_soft)       lbl = 1;
            else if (hu < thr_soft_spongiosa) lbl = 2;
            else if (hu < thr_spongiosa_cort) lbl = 3;
            else                              lbl = 4;
            labelLUT[e.seq_index] = lbl;
        }

        // Optional int16 HU volume (actual per-material mu->HU) for QA / DICOM.
        // hp != nullptr enables HU capture during voxelization + overlays.
        HUDicomExporter::HUImageType::Pointer huImage;
        int16_t* hp = nullptr;
        if (write_dicom) {
            huImage = HUDicomExporter::HUImageType::New();
            HUDicomExporter::HUImageType::SizeType sz;
            sz[0]=out_nx; sz[1]=out_ny; sz[2]=out_nz;
            HUDicomExporter::HUImageType::RegionType reg; reg.SetSize(sz);
            huImage->SetRegions(reg); huImage->Allocate();
            const double spc[3]={out_vxy,out_vxy,out_vz}; huImage->SetSpacing(spc);
            const double org[3]={-out_nx*out_vxy/2.0,-out_ny*out_vxy/2.0,-out_nz*out_vz/2.0};
            huImage->SetOrigin(org);
            hp = huImage->GetBufferPointer();
            std::fill(hp, hp + totalVox, int16_t(-1000));   // air background
            std::cout << "HU capture ON (write_dicom): +"
                      << (totalVox*sizeof(int16_t)/(1024.0*1024.0*1024.0)) << " GiB\n";
        }

        // ---- Voxelize: write the tissue label straight into the output grid ----
        std::vector<uint8_t> vol(totalVox, 0);   // 0 = air background
        std::cout << "Voxelizing " << numTet << " tetrahedra...\n" << std::flush;
        for (G4int t = 0; t < numTet; ++t) {
            if (t % 500000 == 0)
                std::cout << "  tet " << t << " / " << numTet << "\n" << std::flush;
            const int seq = matMap.getSeqIndex(tetImport.GetMaterialIndex(t));
            const uint8_t lbl = labelLUT[seq];
            if (lbl == 0 && !hp) continue;   // air-equivalent tissue: leave as background
                                             // (unless capturing HU, which needs it)

            G4Tet* tet = tetImport.GetTetrahedron(t);
            const auto& vtx = tet->GetVertices();
            double xm=DBL_MAX,xM=-DBL_MAX,ym=DBL_MAX,yM=-DBL_MAX,zm=DBL_MAX,zM=-DBL_MAX;
            for (const auto& v : vtx) {
                xm=std::min(xm,v.x()); xM=std::max(xM,v.x());
                ym=std::min(ym,v.y()); yM=std::max(yM,v.y());
                zm=std::min(zm,v.z()); zM=std::max(zM,v.z());
            }
            // phantom G4 coord -> output index:  idx = (coord - iso + half)/sp - 0.5
            auto lo = [](double c,double iso,double half,double sp,G4int n){
                return std::max(0, (G4int)std::floor((c-iso+half)/sp - 0.5)); };
            auto hi = [](double c,double iso,double half,double sp,G4int n){
                return std::min(n-1, (G4int)std::ceil((c-iso+half)/sp - 0.5)); };
            const G4int i0=lo(xm,isoPx,halfX,out_vxy_g,out_nx), i1=hi(xM,isoPx,halfX,out_vxy_g,out_nx);
            const G4int j0=lo(ym,isoPy,halfY,out_vxy_g,out_ny), j1=hi(yM,isoPy,halfY,out_vxy_g,out_ny);
            const G4int k0=lo(zm,isoPz,halfZ,out_vz_g, out_nz), k1=hi(zM,isoPz,halfZ,out_vz_g, out_nz);
            for (G4int k=k0;k<=k1;++k) {
                const double pz=(k+0.5)*out_vz_g -halfZ+isoPz;
                for (G4int j=j0;j<=j1;++j) {
                    const double py=(j+0.5)*out_vxy_g-halfY+isoPy;
                    for (G4int i=i0;i<=i1;++i) {
                        const double px=(i+0.5)*out_vxy_g-halfX+isoPx;
                        if (tet->Inside(G4ThreeVector(px,py,pz))==kOutside) continue;
                        const size_t f=(size_t)k*slicePx + (size_t)j*out_nx + i;
                        vol[f] = lbl;
                        if (hp) hp[f] = huLUT[seq];
                    }
                }
            }
        }
        std::cout << "Voxelization done.\n";

        // Isocenter-relative coord of an output voxel index (mm)
        auto xISO=[&](G4int i){ return (i+0.5)*out_vxy - out_nx*out_vxy/2.0; };
        auto yISO=[&](G4int j){ return (j+0.5)*out_vxy - out_ny*out_vxy/2.0; };
        auto zISO=[&](G4int k){ return (k+0.5)*out_vz  - out_nz*out_vz /2.0; };

        // ---- Implant cylinders (label 5, axis along Z) ----
        for (size_t n=0;n<implants.size();++n) {
            const auto& imp=implants[n];
            const double r2=imp.radius_mm*imp.radius_mm, halfH=imp.height_mm/2.0;
            size_t hit=0;
            for (G4int k=0;k<out_nz;++k){ if (std::abs(zISO(k)-imp.cz_mm)>halfH) continue;
                for (G4int j=0;j<out_ny;++j){ const double dy=yISO(j)-imp.cy_mm;
                    for (G4int i=0;i<out_nx;++i){ const double dx=xISO(i)-imp.cx_mm;
                        if (dx*dx+dy*dy<=r2){ const size_t f=(size_t)k*slicePx+(size_t)j*out_nx+i;
                            vol[f]=5; if(hp)hp[f]=3000; ++hit; } } } }
            std::cout<<"Implant["<<n<<"]: centre=("<<imp.cx_mm<<","<<imp.cy_mm<<","<<imp.cz_mm
                     <<") r="<<imp.radius_mm<<" h="<<imp.height_mm<<" -> "<<hit<<" voxels (5)\n";
        }

        // ---- Crop cylinder: zero everything outside (label 0) ----
        if (crop_cylinder_enable) {
            const double r2=crop_cylinder_radius_mm*crop_cylinder_radius_mm;
            std::vector<char> keep(slicePx);
            for (G4int j=0;j<out_ny;++j){ const double dy=yISO(j)-crop_cylinder_cy_mm;
                for (G4int i=0;i<out_nx;++i){ const double dx=xISO(i)-crop_cylinder_cx_mm;
                    keep[(size_t)j*out_nx+i]=(dx*dx+dy*dy<=r2); } }
            size_t zeroed=0;
            for (G4int k=0;k<out_nz;++k) for (size_t ij=0;ij<slicePx;++ij)
                if (!keep[ij] && vol[(size_t)k*slicePx+ij]){ const size_t f=(size_t)k*slicePx+ij;
                    vol[f]=0; if(hp)hp[f]=-1000; ++zeroed; }
            std::cout<<"Crop cylinder: r="<<crop_cylinder_radius_mm<<" -> "<<zeroed<<" voxels zeroed\n";
        }

        // ---- Stretcher shell (carbon 10 / foam 11) — same profile as dicom_to_mcgpu ----
        const bool hasStretcher = stretcher_enable;
        if (stretcher_enable) {
            const double S_TOP_W=440.0,S_BOT_W=396.0,S_H=57.0,S_STR_H=20.0,
                         S_WALL_T=1.5,S_CHAM=4.0,S_CORNER_R=5.0;
            const double hw_t=S_TOP_W/2.0,hw_b=S_BOT_W/2.0,ht=S_H/2.0,ky=ht-S_STR_H,c=S_CHAM;
            const Poly2D sharpOuter={{-hw_b,-ht},{hw_b,-ht},{hw_t,ky},{hw_t,ht-c},
                                     {hw_t-c,ht},{-hw_t+c,ht},{-hw_t,ht-c},{-hw_t,ky}};
            const Poly2D outerPoly=roundPolygon(sharpOuter,S_CORNER_R);
            const Poly2D innerPoly=roundPolygon(inwardOffset(sharpOuter,S_WALL_T),
                                                std::max(S_CORNER_R-S_WALL_T,0.0));
            const double str_cx=stretcher.cx_mm;
            // Auto: seat the shell's flat top just behind the head's posterior (+Y)
            // surface (same convention as the consolidated AM/AF placement).
            const double halfDepthY=(hy1-hy0)/mm/2.0;
            const double str_cy = stretcher_auto ? (halfDepthY+stretcher_gap_mm+ht)
                                                 : stretcher.cy_mm;
            std::cout<<"Stretcher cy  : "<<str_cy<<" mm  (cx="<<str_cx<<")  "
                     <<(stretcher_auto?"[auto: halfDepth+gap+halfH]":"[from cfg]")<<"\n";
            std::vector<uint8_t> sm(slicePx,0);
            for (G4int j=0;j<out_ny;++j){ const double dy=str_cy-yISO(j);
                if (dy<-ht||dy>ht) continue;
                for (G4int i=0;i<out_nx;++i){ const double dx=xISO(i)-str_cx;
                    if (dx<-hw_t||dx>hw_t) continue;
                    if (!inConvex(outerPoly,dx,dy)) continue;
                    sm[(size_t)j*out_nx+i]=inConvex(innerPoly,dx,dy)?11:10; } }
            size_t carb=0,foam=0;
            for (G4int k=0;k<out_nz;++k) for (size_t ij=0;ij<slicePx;++ij){
                const uint8_t m=sm[ij]; if(!m) continue;
                const size_t f=(size_t)k*slicePx+ij;
                vol[f]=m; if(hp)hp[f]=(m==11?-100:3000);
                if(m==11)++foam; else ++carb; }
            std::cout<<"Stretcher shell: "<<carb<<" carbon(10) + "<<foam<<" foam(11) voxels\n";
        }

        // ---- Output paths ----
        const std::string dimTag=std::to_string(out_nx)+"x"+std::to_string(out_ny)+"x"+std::to_string(out_nz);
        const std::string folderStem=(outputPrefix==phantomName)?outputPrefix:(phantomName+"_"+outputPrefix);
        const std::string outDir=output_dir+"/"+folderStem+"_vox_"+dimTag+"/";
        std::filesystem::create_directories(outDir);
        const bool hasImplant=!implants.empty();
        const std::string labelTag=std::string(hasImplant?"implant_":"")
                                  +(hasStretcher?"stretcher_":"")
                                  +(crop_cylinder_enable?"reconCylinder_":"")
                                  +((hasImplant||hasStretcher)?"labels_":"5labels_");
        const std::string base=outDir+outputPrefix+"_vox_"+labelTag+dimTag;
        const std::string rawPath=base+".raw";
        { std::ofstream f(rawPath,std::ios::binary);
          f.write(reinterpret_cast<const char*>(vol.data()),(std::streamsize)totalVox); }
        std::cout<<"Label phantom written to: "<<rawPath<<"\n";

        // Label histogram
        { size_t cnt[12]={0}; for(size_t i=0;i<totalVox;++i) if(vol[i]<12) ++cnt[vol[i]];
          const char* nm[12]={"air","fat","soft","spongiosa","cortical","implant","","","","","carbon","foam"};
          std::cout<<std::fixed<<std::setprecision(2)<<"Label distribution:\n";
          for(int l=0;l<12;++l) if(cnt[l]&&nm[l][0])
              std::cout<<"  "<<l<<" ("<<nm[l]<<"): "<<cnt[l]<<"  ("<<100.0*cnt[l]/totalVox<<" %)\n"; }

        // ---- Optional HU int16 raw + DICOM series (write_dicom) ----
        // HU here is the ACTUAL per-material mu@60keV -> HU (continuous), so you
        // can inspect what drives the label thresholds; overlays carry the same
        // HU as dicom_to_mcgpu (implant/carbon 3000, foam -100, air/crop -1000).
        if (hp) {
            const std::string huRaw = outDir+outputPrefix+"_vox_HU60keV_"+dimTag+".raw";
            { std::ofstream f(huRaw,std::ios::binary);
              f.write(reinterpret_cast<const char*>(hp),
                      (std::streamsize)(totalVox*sizeof(int16_t))); }
            std::cout<<"HU int16 raw written to: "<<huRaw<<"\n";
            HUDicomExporter::Options opt;
            opt.patientName       = phantomName;
            opt.seriesDescription = phantomName+" HU @ 60 keV";
            const std::string huDir = outDir+"HU60keV_dicom_"+dimTag;
            HUDicomExporter::Write(huImage, huDir, opt);
            std::cout<<"HU DICOM series written to: "<<huDir<<"\n";
        }

        // ---- .txt companion (penEasy 2008 header) ----
        const double sp_xy=out_vxy/10.0, sp_z=out_vz/10.0;   // cm
        const double off_x=-out_nx*sp_xy/2.0 + shift_x_mm/10.0;
        const double off_y=-out_ny*sp_xy/2.0 + shift_y_mm/10.0;
        const double off_z=-out_nz*sp_z /2.0 + shift_z_mm/10.0;
        { std::ofstream info(base+".txt"); info<<std::fixed<<std::setprecision(3);
          info<<"#[SECTION VOXELIZED GEOMETRY FILE v.2017-07-26]\n";
          info<<"phantom/"<<std::filesystem::path(base).filename().string()
              <<".raw     # VOXEL GEOMETRY FILE (penEasy 2008 format; .gz accepted)\n";
          info<<" "<<off_x<<"  "<<off_y<<"  "<<off_z<<"              # OFFSET OF THE VOXEL GEOMETRY [cm]\n";
          info<<" "<<out_nx<<" "<<out_ny<<" "<<out_nz<<"                 # NUMBER OF VOXELS\n";
          info<<" "<<sp_xy<<" "<<sp_xy<<" "<<sp_z<<"           # VOXEL SIZES [cm]\n";
          info<<" 2 2 2                          # SIZE OF LOW RESOLUTION VOXELS\n";
        }
        std::cout<<"Label info file written: "<<base<<".txt\n";

        // ---- MC-GPU .in file(s) — one per template (','/';' separated) ----
        if (!mcgpu_in_template.empty()) {
            std::vector<std::string> templates;
            { std::string item; auto flush=[&](){
                item.erase(0,item.find_first_not_of(" \t\r\n"));
                const auto last=item.find_last_not_of(" \t\r\n");
                if(last!=std::string::npos) item.erase(last+1); else item.clear();
                if(!item.empty()) templates.push_back(item); item.clear(); };
              for(char ch:mcgpu_in_template){ if(ch==','||ch==';') flush(); else item+=ch; } flush(); }
            const bool multi=templates.size()>1;
            const std::string geomRef="phantom/"+std::filesystem::path(base).filename().string()+".raw";
            for (const std::string& tmplPath:templates) {
                std::ifstream tmpl(tmplPath);
                if(!tmpl){ std::cerr<<"Warning: cannot open MC-GPU template: "<<tmplPath<<"\n"; continue; }
                std::string stem=std::filesystem::path(tmplPath).stem().string();
                const std::string inPath = multi?(outDir+"CBCT_"+stem+".in"):(outDir+"CBCT.in");
                const std::string outName= multi?(mcgpu_output_name+stem):mcgpu_output_name;
                std::ofstream out(inPath);
                if(!out) throw std::runtime_error("Cannot write .in file: "+inPath);
                std::string line; int skip=0;
                while(std::getline(tmpl,line)){
                    if(line.find("#[SECTION IMAGE DETECTOR")!=std::string::npos){
                        out<<line<<"\n";
                        out<<outName<<"   # OUTPUT IMAGE FILE NAME\n";
                        out<<mcgpu_det_nx<<"      "<<mcgpu_det_nz
                           <<"                  # NUMBER OF PIXELS IN THE IMAGE: Nx Nz\n";
                        skip=2;
                    } else if(line.find("#[SECTION VOXELIZED GEOMETRY FILE")!=std::string::npos){
                        out<<line<<"\n"; out<<std::fixed<<std::setprecision(3);
                        out<<geomRef<<"     # VOXEL GEOMETRY FILE\n";
                        out<<" "<<off_x<<"  "<<off_y<<"  "<<off_z<<"              # OFFSET OF THE VOXEL GEOMETRY [cm]\n";
                        out<<" "<<out_nx<<" "<<out_ny<<" "<<out_nz<<"                 # NUMBER OF VOXELS\n";
                        out<<" "<<sp_xy<<" "<<sp_xy<<" "<<sp_z<<"           # VOXEL SIZES [cm]\n";
                        skip=4;   // template's low-res-voxels line passes through
                    } else if(skip>0){ --skip; }
                    else { out<<line<<"\n"; }
                }
                std::cout<<"MC-GPU .in file written: "<<inPath<<"  (from "<<tmplPath<<")\n";
            }
        }

        std::cout<<"Done.\n";
        return 0;
    }

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
        // Folder name carries the phantom name: <phantom>_<prefix>_vox_<dims>
        // (when the prefix already is the phantom name, don't duplicate it).
        const std::string folderStem = (outputPrefix == phantomName)
            ? outputPrefix
            : phantomName + "_" + outputPrefix;
        const std::string outDir = output_dir + "/" + folderStem + "_vox_" + dimTag + "/";
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
    matMap.writeMCGPUSectionFile(outputBase + "mcgpu_section.txt", phantomName);

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

    // Isocenter of the slice ROI in absolute phantom coords (from bbMin corner).
    // Implant and stretcher cfg coords are isocenter-relative (same convention as dicom_to_mcgpu).
    const double iso_x = (iStart + nx_slice / 2.0) * voxelSize_mm;
    const double iso_y = (jStart + ny_slice / 2.0) * voxelSize_mm;
    const double iso_z = (kStart + nz_slice / 2.0) * voxelSize_mm;

    // -- Implant cylinder override (axis along Z) --
    const bool hasImplant = !implants.empty();
    for (size_t impIdx = 0; impIdx < implants.size(); ++impIdx)
    {
        const auto& imp = implants[impIdx];
        const double r2    = imp.radius_mm * imp.radius_mm;
        const double halfH = imp.height_mm / 2.0;
        // Convert isocenter-relative → bbMin-absolute
        const double cx_abs = iso_x + imp.cx_mm;
        const double cy_abs = iso_y + imp.cy_mm;
        const double cz_abs = iso_z + imp.cz_mm;
        size_t implantVoxels = 0;

        for (G4int k = 0; k < nz_slice; ++k)
        {
            const double vz = (kStart + k + 0.5) * voxelSize_mm;
            if (std::abs(vz - cz_abs) > halfH) continue;

            for (G4int j = 0; j < ny_slice; ++j)
            {
                const double vy = (jStart + j + 0.5) * voxelSize_mm;
                const double dy = vy - cy_abs;

                for (G4int i = 0; i < nx_slice; ++i)
                {
                    const double vx = (iStart + i + 0.5) * voxelSize_mm;
                    const double dx = vx - cx_abs;

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
        std::cout << "Implant[" << impIdx << "]: centre=(" << imp.cx_mm << ", "
                  << imp.cy_mm << ", " << imp.cz_mm << ") mm from isocenter"
                  << "  r=" << imp.radius_mm << " mm"
                  << "  h=" << imp.height_mm << " mm"
                  << "  → " << implantVoxels << " voxels labelled 5" << std::endl;
    }

    // -- Crop cylinder mask (zero everything outside) --
    if (crop_cylinder_enable)
    {
        // Convert isocenter-relative → bbMin-absolute (same convention as the
        // implant / stretcher above and as crop_cylinder in dicom_to_mcgpu).
        const double cx = iso_x + crop_cylinder_cx_mm;
        const double cy = iso_y + crop_cylinder_cy_mm;
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

    // Final volume dimensions (may grow if stretcher pads the volume)
    G4int fin_nx_slice = nx_slice;
    G4int fin_ny_slice = ny_slice;

    // -- Stretcher shell: rounded polygon cross-section (ported from dicom_to_mcgpu) --
    // stretcher_cx_mm / cy_mm are isocenter-relative [mm], matching the (cx, cy)
    // reported by the stretcher-fitting tools (same convention as dicom_to_mcgpu).
    // The volume is zero-padded in X and/or Y if the stretcher falls outside the slice ROI.
    const bool hasStretcher = stretcher_enable;
    if (stretcher_enable)
    {
        // Cross-section dimensions [mm] (same hard-coded profile as dicom_to_mcgpu)
        const double S_TOP_W    = 440.0;
        const double S_BOT_W    = 396.0;
        const double S_H        =  57.0;
        const double S_STR_H    =  20.0;
        const double S_WALL_T   =   1.5;
        const double S_CHAM     =   4.0;
        const double S_CORNER_R =   5.0;

        const double hw_t = S_TOP_W/2.0, hw_b = S_BOT_W/2.0;
        const double ht = S_H/2.0, ky = ht-S_STR_H, c = S_CHAM;

        const Poly2D sharpOuter = {
            {-hw_b,-ht},{hw_b,-ht},{hw_t,ky},{hw_t,ht-c},
            {hw_t-c,ht},{-hw_t+c,ht},{-hw_t,ht-c},{-hw_t,ky},
        };
        const Poly2D outerPoly = roundPolygon(sharpOuter, S_CORNER_R);
        const Poly2D innerPoly = roundPolygon(inwardOffset(sharpOuter, S_WALL_T),
                                              std::max(S_CORNER_R-S_WALL_T, 0.0));

        // Stretcher bounding box in absolute phantom coords [mm]
        const double str_x_min = iso_x + stretcher.cx_mm - hw_t;
        const double str_x_max = iso_x + stretcher.cx_mm + hw_t;
        const double str_y_min = iso_y + stretcher.cy_mm - ht;
        const double str_y_max = iso_y + stretcher.cy_mm + ht;

        // Current buffer X/Y range in absolute coords
        const double buf_x_min = static_cast<double>(iStart) * voxelSize_mm;
        const double buf_x_max = static_cast<double>(iStart + nx_slice) * voxelSize_mm;
        const double buf_y_min = static_cast<double>(jStart) * voxelSize_mm;
        const double buf_y_max = static_cast<double>(jStart + ny_slice) * voxelSize_mm;

        const int pad_xl = (str_x_min < buf_x_min)
            ? static_cast<int>(std::ceil((buf_x_min - str_x_min) / voxelSize_mm)) : 0;
        const int pad_xr = (str_x_max > buf_x_max)
            ? static_cast<int>(std::ceil((str_x_max - buf_x_max) / voxelSize_mm)) : 0;
        const int pad_yb = (str_y_min < buf_y_min)
            ? static_cast<int>(std::ceil((buf_y_min - str_y_min) / voxelSize_mm)) : 0;
        const int pad_yt = (str_y_max > buf_y_max)
            ? static_cast<int>(std::ceil((str_y_max - buf_y_max) / voxelSize_mm)) : 0;

        const G4int new_nx_slice = nx_slice + pad_xl + pad_xr;
        const G4int new_ny_slice = ny_slice + pad_yb + pad_yt;

        if (pad_xl || pad_xr || pad_yb || pad_yt)
        {
            std::cout << "Stretcher: padding volume ("
                      << pad_xl << "+" << pad_xr << ") x ("
                      << pad_yb << "+" << pad_yt << ") voxels in X / Y." << std::endl;

            const size_t newTotal = static_cast<size_t>(new_nx_slice) *
                                    static_cast<size_t>(new_ny_slice) *
                                    static_cast<size_t>(nz_slice);
            std::vector<uint8_t> newBuf(newTotal, 0);
            for (G4int k = 0; k < nz_slice; ++k)
                for (G4int j = 0; j < ny_slice; ++j)
                    for (G4int i = 0; i < nx_slice; ++i)
                        newBuf[static_cast<size_t>(k) * new_ny_slice * new_nx_slice
                               + static_cast<size_t>(j + pad_yb) * new_nx_slice
                               + static_cast<size_t>(i + pad_xl)]
                            = labelBuf[static_cast<size_t>(k) * ny_slice * nx_slice
                                       + static_cast<size_t>(j) * nx_slice
                                       + static_cast<size_t>(i)];
            labelBuf = std::move(newBuf);

            // Keep the isocenter of the original slice ROI fixed in MC-GPU world coords.
            shift_x_mm += (static_cast<double>(pad_xr) - static_cast<double>(pad_xl)) * voxelSize_mm / 2.0;
            shift_y_mm += (static_cast<double>(pad_yt) - static_cast<double>(pad_yb)) * voxelSize_mm / 2.0;
        }

        // 2D mask over the padded grid: 0=outside, 10=carbon wall, 11=foam core.
        // Same coordinate formula as dicom_to_mcgpu: the polygon is defined with
        // +Y toward the patient, so dy = str_cy - y_rel (y_rel grows toward
        // posterior / increasing j in this frame, as in the DICOM LPS frame).
        std::vector<uint8_t> strMask(static_cast<size_t>(new_nx_slice) * new_ny_slice, 0);
        for (G4int j = 0; j < new_ny_slice; ++j)
        {
            const double y_rel = (static_cast<double>(jStart - pad_yb + j) + 0.5) * voxelSize_mm - iso_y;
            const double dy = stretcher.cy_mm - y_rel;
            if (dy < -ht || dy > ht) continue;
            for (G4int i = 0; i < new_nx_slice; ++i)
            {
                const double x_rel = (static_cast<double>(iStart - pad_xl + i) + 0.5) * voxelSize_mm - iso_x;
                const double dx = x_rel - stretcher.cx_mm;
                if (dx < -hw_t || dx > hw_t) continue;
                if (!inConvex(outerPoly, dx, dy)) continue;
                strMask[static_cast<size_t>(j) * new_nx_slice + i] =
                    inConvex(innerPoly, dx, dy) ? 11 : 10;
            }
        }

        // Apply the mask to every Z slice (stretcher spans the full Z range,
        // as in dicom_to_mcgpu).
        size_t foamVox = 0, carbVox = 0;
        const size_t slicePx = static_cast<size_t>(new_nx_slice) * new_ny_slice;
        for (G4int k = 0; k < nz_slice; ++k)
            for (size_t ij = 0; ij < slicePx; ++ij)
            {
                const uint8_t m = strMask[ij];
                if (m == 0) continue;
                labelBuf[static_cast<size_t>(k) * slicePx + ij] = m;
                if (m == 11) ++foamVox; else ++carbVox;
            }

        std::cout << "Stretcher shell: centre=(" << stretcher.cx_mm << ", " << stretcher.cy_mm
                  << ") mm from isocenter"
                  << "  carbon wall=" << S_WALL_T << " mm (label 10)"
                  << "  foam core (label 11)"
                  << "  -> " << carbVox << " carbon + " << foamVox << " foam voxels" << std::endl;
        std::cout << "Updated volume: " << new_nx_slice << " x " << new_ny_slice << " x " << nz_slice << std::endl;
        std::cout << "Updated shift : (" << shift_x_mm << ", " << shift_y_mm << ") mm" << std::endl;

        fin_nx_slice = new_nx_slice;
        fin_ny_slice = new_ny_slice;
    }

    // -- Write raw --
    const std::string labelTag = std::string(hasImplant      ? "implant_"      : "")
                               + (hasStretcher               ? "stretcher_"    : "")
                               + (crop_cylinder_enable       ? "reconCylinder_": "")
                               + (hasImplant || hasStretcher ? "labels_"       : "5labels_");
    const size_t finalTotalVox = static_cast<size_t>(fin_nx_slice) *
                                 static_cast<size_t>(fin_ny_slice) *
                                 static_cast<size_t>(nz_slice);
    std::string label4Path = outputBase + labelTag +
                             std::to_string(fin_nx_slice) + "x" +
                             std::to_string(fin_ny_slice) + "x" +
                             std::to_string(nz_slice) + ".raw";
    {
        std::ofstream label4File(label4Path, std::ios::binary);
        label4File.write(reinterpret_cast<const char*>(labelBuf.data()),
                         static_cast<std::streamsize>(finalTotalVox));
    }
    std::cout << "Label phantom written to: " << label4Path << std::endl;

    // -- Label histogram (same summary as dicom_to_mcgpu) --
    {
        size_t counts[12] = {0};
        for (size_t i = 0; i < finalTotalVox; ++i)
            if (labelBuf[i] < 12) ++counts[labelBuf[i]];
        const char* names[12]={"air","fat","soft","spongiosa","cortical","implant",
                                "","","","","carbon","foam"};
        std::cout<<std::fixed<<std::setprecision(2)<<"Label distribution:\n";
        for (int l=0;l<12;++l)
            if (counts[l]>0 && names[l][0]!='\0')
                std::cout<<"  "<<l<<" ("<<names[l]<<"): "<<counts[l]
                         <<"  ("<<100.0*counts[l]/finalTotalVox<<" %)\n";
    }

    // write companion info .txt
    {
        const std::string infoBase = outputBase + labelTag +
                                     std::to_string(fin_nx_slice) + "x" +
                                     std::to_string(fin_ny_slice) + "x" +
                                     std::to_string(nz_slice);
        const double sp = voxelSize_mm / 10.0;  // cm
        std::ofstream info4(infoBase + ".txt");
        info4 << std::fixed << std::setprecision(3);
        info4 << "#[SECTION VOXELIZED GEOMETRY FILE v.2017-07-26]\n";
        info4 << "phantom/" << std::filesystem::path(infoBase).filename().string()
              << ".raw     # VOXEL GEOMETRY FILE (penEasy 2008 format; .gz accepted)\n";
        info4 << " " << (-static_cast<double>(fin_nx_slice) * sp / 2.0 + shift_x_mm / 10.0)
              << "  " << (-static_cast<double>(fin_ny_slice) * sp / 2.0 + shift_y_mm / 10.0)
              << "  " << (-static_cast<double>(nz_slice) * sp / 2.0 + shift_z_mm / 10.0)
              << "              # OFFSET OF THE VOXEL GEOMETRY [cm]\n";
        info4 << " " << fin_nx_slice << " " << fin_ny_slice << " " << nz_slice
              << "                 # NUMBER OF VOXELS\n";
        info4 << " " << sp << " " << sp << " " << sp
              << "           # VOXEL SIZES [cm]\n";
        info4 << " 2 2 2                          # SIZE OF LOW RESOLUTION VOXELS\n";
        std::cout << "Label info file written: " << infoBase << ".txt" << std::endl;
    }

    // -------------------------------------------------------------------------
    // Write MC-GPU .in file(s)  (same conventions as dicom_to_mcgpu)
    // -------------------------------------------------------------------------
    // mcgpu_in_template may list several templates separated by ',' or ';'.
    // The volume is built only once above; here we emit one .in per template.
    // With a single template the output is "CBCT.in"; with several, each is
    // "CBCT_<template-stem>.in" and its OUTPUT IMAGE FILE NAME is suffixed
    // with the stem to avoid collisions.
    if (!mcgpu_in_template.empty())
    {
        std::vector<std::string> templates;
        {
            std::string item;
            auto flush = [&]() {
                item.erase(0, item.find_first_not_of(" \t\r\n"));
                const auto last = item.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) item.erase(last + 1); else item.clear();
                if (!item.empty()) templates.push_back(item);
                item.clear();
            };
            for (char c : mcgpu_in_template) {
                if (c == ',' || c == ';') flush();
                else item += c;
            }
            flush();
        }
        const bool multi = templates.size() > 1;

        const std::string outDir = std::filesystem::path(label4Path).parent_path().string() + "/";
        const double sp_cm = voxelSize_mm / 10.0;
        const double off_x = -static_cast<double>(fin_nx_slice) * sp_cm / 2.0 + shift_x_mm / 10.0;
        const double off_y = -static_cast<double>(fin_ny_slice) * sp_cm / 2.0 + shift_y_mm / 10.0;
        const double off_z = -static_cast<double>(nz_slice)     * sp_cm / 2.0 + shift_z_mm / 10.0;
        // VOXEL GEOMETRY FILE: reference the .raw by name under phantom/
        // (matches the on-disk basename), never the absolute disk path.
        const std::string geomRef = "phantom/" + std::filesystem::path(label4Path).filename().string();

        for (const std::string &tmplPath : templates)
        {
            std::ifstream tmpl(tmplPath);
            if (!tmpl) {
                std::cerr << "Warning: cannot open MC-GPU template: " << tmplPath << std::endl;
                continue;
            }
            // stem = file name without directory or extension
            const auto slash = tmplPath.find_last_of("/\\");
            std::string stem = (slash == std::string::npos) ? tmplPath
                                                            : tmplPath.substr(slash + 1);
            const auto dot = stem.find_last_of('.');
            if (dot != std::string::npos) stem.erase(dot);

            const std::string inPath  = multi ? (outDir + "CBCT_" + stem + ".in")
                                              : (outDir + "CBCT.in");
            const std::string outName = multi ? (mcgpu_output_name + stem)
                                              : mcgpu_output_name;

            std::ofstream out(inPath);
            if (!out) throw std::runtime_error("Cannot write .in file: " + inPath);

            std::string line;
            int skipLines = 0;
            while (std::getline(tmpl, line))
            {
                if (line.find("#[SECTION IMAGE DETECTOR") != std::string::npos)
                {
                    out << line << "\n";
                    out << outName << "   # OUTPUT IMAGE FILE NAME\n";
                    out << mcgpu_det_nx << "      " << mcgpu_det_nz
                        << "                  # NUMBER OF PIXELS IN THE IMAGE: Nx Nz\n";
                    skipLines = 2;
                }
                else if (line.find("#[SECTION VOXELIZED GEOMETRY FILE") != std::string::npos)
                {
                    out << line << "\n";
                    out << std::fixed << std::setprecision(3);
                    out << geomRef << "     # VOXEL GEOMETRY FILE\n";
                    out << " " << off_x << "  " << off_y << "  " << off_z
                        << "              # OFFSET OF THE VOXEL GEOMETRY [cm]\n";
                    out << " " << fin_nx_slice << " " << fin_ny_slice << " " << nz_slice
                        << "                 # NUMBER OF VOXELS\n";
                    out << " " << sp_cm << " " << sp_cm << " " << sp_cm
                        << "           # VOXEL SIZES [cm]\n";
                    // Replace only the 4 lines we recompute; the template's
                    // "SIZE OF LOW RESOLUTION VOXELS" line passes through verbatim.
                    skipLines = 4;
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
            std::cout << "MC-GPU .in file written: " << inPath
                      << "  (from " << tmplPath << ")" << std::endl;
        }
    }

    std::cout << "Done." << std::endl;
    return 0;
}
