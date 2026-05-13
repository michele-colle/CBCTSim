// dicom_to_mcgpu.cpp
// Reads a DICOM CT series, places it into a user-defined output volume with
// optional 3D rotation, adds stretcher / implant overlays, and writes:
//   - MC-GPU uint8 label .raw + companion .txt
//   - MC-GPU .in file (if template provided)
//   - DICOM output folder with HU values (if dicom_output_dir is set)
//
// COORDINATE SYSTEM: all positions in mm, origin = ISOCENTER = centre of
// the output volume.  Voxel [i,j,k] centre:
//   x = (i+0.5)*vx - Nx*vx/2,   y = ...,   z = ...
//
// Usage: dicom_to_mcgpu <cfg_file>
//        dicom_to_mcgpu <dicom_dir> [output_prefix]
//
// Label scheme:
//   0=air  1=fat  2=soft  3=spongiosa  4=cortical  5=implant
//   10=carbon(stretcher)  11=foam(stretcher)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageSeriesReader.h>
#include <itkMetaDataObject.h>
#include <itkNumericSeriesFileNames.h>

#include <gdcmDataElement.h>
#include <gdcmImageReader.h>
#include <gdcmImageWriter.h>
#include <gdcmPhotometricInterpretation.h>
#include <gdcmPixelFormat.h>
#include <gdcmTag.h>
#include <gdcmUIDGenerator.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Convert Windows/WSL paths → Linux paths:
//   C:\foo\bar  →  /mnt/c/foo/bar
//   C:/foo/bar  →  /mnt/c/foo/bar
// Paths already starting with / are returned unchanged.
// ---------------------------------------------------------------------------
static std::string toLinuxPath(std::string p)
{
    // Backslashes → forward slashes
    std::replace(p.begin(), p.end(), '\\', '/');
    // Drive letter:  X:/...  →  /mnt/x/...
    if (p.size() >= 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':') {
        p[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(p[0])));
        p = "/mnt/" + p.substr(0, 1) + p.substr(2); // e.g. "/mnt/f/..."
    }
    return p;
}

// ---------------------------------------------------------------------------
// Config-file parser
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
// Implant cylinder (axis along Z, coords in isocenter mm)
// ---------------------------------------------------------------------------
struct ImplantCylinder {
    double cx_mm, cy_mm, cz_mm, radius_mm, height_mm;
};

// ---------------------------------------------------------------------------
// Stretcher shell (coords in isocenter mm)
// ---------------------------------------------------------------------------
struct StretcherShell {
    // Position of the centre of the flat top surface, measured from the
    // minimum-X / minimum-Y corner of the output volume.
    double top_x_mm = 0.0;  // distance from left edge of volume to stretcher top-centre X [mm]
    double top_y_mm = 0.0;  // distance from bottom edge of volume to stretcher top-centre Y [mm]
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

// ---------------------------------------------------------------------------
// 3×3 rotation matrix (used for DICOM placement)
// ---------------------------------------------------------------------------
struct Mat3 {
    double m[3][3];
    Mat3() { for (int i=0;i<3;i++) for (int j=0;j<3;j++) m[i][j]=0.0; }

    std::array<double,3> apply(double x, double y, double z) const {
        return { m[0][0]*x+m[0][1]*y+m[0][2]*z,
                 m[1][0]*x+m[1][1]*y+m[1][2]*z,
                 m[2][0]*x+m[2][1]*y+m[2][2]*z };
    }
    Mat3 operator*(const Mat3& B) const {
        Mat3 C;
        for (int i=0;i<3;i++) for (int j=0;j<3;j++)
            for (int k=0;k<3;k++) C.m[i][j] += m[i][k]*B.m[k][j];
        return C;
    }
    Mat3 T() const {
        Mat3 R;
        for (int i=0;i<3;i++) for (int j=0;j<3;j++) R.m[i][j]=m[j][i];
        return R;
    }
};

static Mat3 rotX(double a) {
    Mat3 R; double c=std::cos(a),s=std::sin(a);
    R.m[0][0]=1; R.m[1][1]=c; R.m[1][2]=-s; R.m[2][1]=s; R.m[2][2]=c;
    return R;
}
static Mat3 rotY(double a) {
    Mat3 R; double c=std::cos(a),s=std::sin(a);
    R.m[0][0]=c; R.m[0][2]=s; R.m[1][1]=1; R.m[2][0]=-s; R.m[2][2]=c;
    return R;
}
static Mat3 rotZ(double a) {
    Mat3 R; double c=std::cos(a),s=std::sin(a);
    R.m[0][0]=c; R.m[0][1]=-s; R.m[1][0]=s; R.m[1][1]=c; R.m[2][2]=1;
    return R;
}

// ---------------------------------------------------------------------------
// Write MC-GPU companion .txt
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
    f << " " << -(static_cast<double>(nx)*sx_cm/2.0)+shift_x_cm
      << "  " << -(static_cast<double>(ny)*sy_cm/2.0)+shift_y_cm
      << "  " << -(static_cast<double>(nz)*sz_cm/2.0)+shift_z_cm
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
    // 0. Working directory
    // -------------------------------------------------------------------------
    std::cout << "Working directory: " << fs::current_path().string() << "\n";

    // -------------------------------------------------------------------------
    // 1. Parameters
    // -------------------------------------------------------------------------
    std::string dicomDir     = ".";
    std::string outputPrefix = "dicom";
    std::string seriesUID;

    // HU thresholds
    int16_t thr_air_fat        = -500;
    int16_t thr_fat_soft       =  -50;
    int16_t thr_soft_spongiosa =  200;
    int16_t thr_spongiosa_cort =  800;

    // Output volume geometry (0 = derive from DICOM)
    double vol_vxy_mm    = 0.0;
    double vol_vz_mm     = 0.0;
    double vol_width_mm  = 0.0;
    double vol_height_mm = 0.0;
    double vol_length_mm = 0.0;

    // DICOM placement in output volume (isocenter coords of DICOM [0,0,0] corner)
    bool   dicom_corner_specified = false;
    double dicom_corner_x_mm = 0.0;
    double dicom_corner_y_mm = 0.0;
    double dicom_corner_z_mm = 0.0;

    // DICOM rotation (XYZ Euler angles in degrees, pivot = DICOM centre)
    double dicom_rot_x_deg = 0.0;
    double dicom_rot_y_deg = 0.0;
    double dicom_rot_z_deg = 0.0;

    // Fine offset applied only to the MC-GPU .txt geometry offset [mm]
    double shift_x_mm = 0.0;
    double shift_y_mm = 0.0;
    double shift_z_mm = 0.0;

    // Crop cylinder (isocenter coords)
    bool   crop_cylinder_enable    = false;
    double crop_cylinder_radius_mm = 100.0;
    double crop_cylinder_cx_mm     = 0.0;
    double crop_cylinder_cy_mm     = 0.0;

    // Implants and stretcher
    std::vector<ImplantCylinder> implants;
    bool           stretcher_enable = false;
    StretcherShell stretcher;

    // MC-GPU .in template
    std::string mcgpu_in_template;
    std::string mcgpu_output_name;
    int         mcgpu_det_nx = 512;
    int         mcgpu_det_nz = 512;

    // DICOM output
    bool        write_dicom     = true;
    std::string dicom_output_dir;   // derived automatically after outDirPath is known

    // NRRD segmentation mask (optional): voxels where mask==0 are forced to air
    std::string mask_nrrd_path;

    const bool usingCfg = (argc == 2) &&
        (std::string(argv[1]).size() > 4) &&
        (std::string(argv[1]).substr(std::string(argv[1]).size()-4) == ".cfg");

    if (usingCfg)
    {
        auto cfg = loadCfg(argv[1]);
        auto getD = [&](const std::string &k, double def) {
            return cfg.count(k) ? std::stod(cfg[k]) : def;
        };
        auto getS = [&](const std::string &k, const std::string &def) {
            return cfg.count(k) ? cfg[k] : def;
        };

        dicomDir = toLinuxPath(getS("dicom_dir", dicomDir));
        outputPrefix = getS("output_prefix", outputPrefix);
        seriesUID    = getS("series_uid",    seriesUID);

        thr_air_fat        = static_cast<int16_t>(getD("thr_air_fat",        thr_air_fat));
        thr_fat_soft       = static_cast<int16_t>(getD("thr_fat_soft",       thr_fat_soft));
        thr_soft_spongiosa = static_cast<int16_t>(getD("thr_soft_spongiosa", thr_soft_spongiosa));
        thr_spongiosa_cort = static_cast<int16_t>(getD("thr_spongiosa_cort", thr_spongiosa_cort));

        vol_vxy_mm    = getD("vol_vxy_mm",    0.0);
        vol_vz_mm     = getD("vol_vz_mm",     0.0);
        vol_width_mm  = getD("vol_width_mm",  0.0);
        vol_height_mm = getD("vol_height_mm", 0.0);
        vol_length_mm = getD("vol_length_mm", 0.0);

        if (cfg.count("dicom_corner_x_mm")) { dicom_corner_x_mm = std::stod(cfg["dicom_corner_x_mm"]); dicom_corner_specified = true; }
        if (cfg.count("dicom_corner_y_mm")) { dicom_corner_y_mm = std::stod(cfg["dicom_corner_y_mm"]); dicom_corner_specified = true; }
        if (cfg.count("dicom_corner_z_mm")) { dicom_corner_z_mm = std::stod(cfg["dicom_corner_z_mm"]); dicom_corner_specified = true; }

        dicom_rot_x_deg = getD("dicom_rot_x_deg", 0.0);
        dicom_rot_y_deg = getD("dicom_rot_y_deg", 0.0);
        dicom_rot_z_deg = getD("dicom_rot_z_deg", 0.0);

        shift_x_mm = getD("shift_x_mm", 0.0);
        shift_y_mm = getD("shift_y_mm", 0.0);
        shift_z_mm = getD("shift_z_mm", 0.0);

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
                imp.cx_mm     = getD(p+"cx_mm",     0.0);
                imp.cy_mm     = getD(p+"cy_mm",     0.0);
                imp.cz_mm     = getD(p+"cz_mm",     0.0);
                imp.radius_mm = getD(p+"radius_mm", 0.0);
                imp.height_mm = getD(p+"height_mm", 0.0);
                implants.push_back(imp);
            }
        }
        else
        {
            const double r = getD("implant_radius_mm", -1.0);
            if (r > 0.0) {
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
        stretcher.top_x_mm = getD("stretcher_top_x_mm", stretcher.top_x_mm);
        stretcher.top_y_mm = getD("stretcher_top_y_mm", stretcher.top_y_mm);

        mcgpu_in_template = getS("mcgpu_in_template", mcgpu_in_template);
        mcgpu_output_name = getS("mcgpu_output_name", mcgpu_output_name);
        mcgpu_det_nx      = static_cast<int>(getD("mcgpu_det_nx", mcgpu_det_nx));
        mcgpu_det_nz      = static_cast<int>(getD("mcgpu_det_nz", mcgpu_det_nz));

        write_dicom = (getS("write_dicom", write_dicom ? "true" : "false") != "false");

        mask_nrrd_path = toLinuxPath(getS("mask_nrrd", ""));

        std::cout << "Parameters loaded from: " << argv[1] << "\n";
    }
    else
    {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <dicom_dir|cfg_file> [output_prefix]\n";
            return 1;
        }
        dicomDir = toLinuxPath(argv[1]);
        if (argc > 2) outputPrefix = argv[2];
    }

    std::cout << "DICOM directory : " << dicomDir     << "\n";
    std::cout << "Output prefix   : " << outputPrefix << "\n";

    // -------------------------------------------------------------------------
    // 2. Load DICOM series
    // -------------------------------------------------------------------------
    using PixelType    = int16_t;
    using ImageType    = itk::Image<PixelType, 3>;
    using ReaderType   = itk::ImageSeriesReader<ImageType>;
    using NamesGenType = itk::GDCMSeriesFileNames;
    using GDCMIOType   = itk::GDCMImageIO;

    // Try to locate a DICOM series.  Tries several strategies in order:
    //   1. Single-dir scan of dicomDir (with series-detail grouping)
    //   2. Same, without series-detail grouping (helps some vendors)
    //   3. Walk every subdirectory and repeat strategies 1+2
    // This is more reliable than SetRecursive(true) on NTFS/WSL mounts.
    const std::string userSeriesUID = seriesUID;  // user-specified UID, may be empty

    auto tryDir = [&](const std::string& dir, bool useDetails) -> std::vector<std::string>
    {
        auto gen = NamesGenType::New();
        gen->SetDirectory(dir);
        gen->SetRecursive(false);
        gen->SetUseSeriesDetails(useDetails);
        gen->SetLoadSequences(false);
        gen->SetLoadPrivateTags(false);
        const auto& uids = gen->GetSeriesUIDs();
        if (uids.empty()) return {};

        // Pick UID: prefer user-specified; otherwise take first found
        std::string uid = (!userSeriesUID.empty() &&
                           std::find(uids.begin(), uids.end(), userSeriesUID) != uids.end())
                          ? userSeriesUID
                          : uids.begin()->c_str();
        if (uids.size() > 1 && userSeriesUID.empty())
            std::cout << "Multiple series in " << dir << "; using first: " << uid << "\n";

        const auto& fn = gen->GetFileNames(uid.c_str());
        if (fn.empty()) return {};
        seriesUID = uid;
        return std::vector<std::string>(fn.begin(), fn.end());
    };

    std::vector<std::string> fileNames;
    std::string foundDir;

    // Strategy 1 & 2: root dir
    for (bool details : {true, false}) {
        fileNames = tryDir(dicomDir, details);
        if (!fileNames.empty()) { foundDir = dicomDir; break; }
    }

    // Strategy 3: walk subdirectories
    if (fileNames.empty()) {
        std::cout << "No series in root; scanning subdirectories...\n";
        for (const auto& entry : fs::recursive_directory_iterator(
                 dicomDir, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_directory()) continue;
            for (bool details : {true, false}) {
                fileNames = tryDir(entry.path().string(), details);
                if (!fileNames.empty()) { foundDir = entry.path().string(); break; }
            }
            if (!fileNames.empty()) break;
        }
    }

    if (fileNames.empty())
        throw std::runtime_error("No DICOM series found in or under: " + dicomDir);

    std::cout << "Series found in : " << foundDir        << "\n";
    std::cout << "Series UID      : " << seriesUID       << "\n";
    std::cout << "Slices found    : " << fileNames.size() << "\n";

    auto gdcmIO = GDCMIOType::New();
    auto reader = ReaderType::New();
    reader->SetImageIO(gdcmIO);
    reader->SetFileNames(fileNames);
    reader->ForceOrthogonalDirectionOff();
    reader->Update();

    ImageType::Pointer huImage = reader->GetOutput();
    huImage->DisconnectPipeline();

    const auto& dcmOriginITK = huImage->GetOrigin();
    const double dcm_orig_x = dcmOriginITK[0];
    const double dcm_orig_y = dcmOriginITK[1];
    const double dcm_orig_z = dcmOriginITK[2];

    const auto &dcmSize    = huImage->GetLargestPossibleRegion().GetSize();
    const auto &dcmSpacing = huImage->GetSpacing();

    const size_t nx_d  = dcmSize[0];
    const size_t ny_d  = dcmSize[1];
    const size_t nz_d  = dcmSize[2];
    const double sx_mm = dcmSpacing[0];
    const double sy_mm = dcmSpacing[1];
    const double sz_mm = dcmSpacing[2];

    std::cout << "DICOM size      : " << nx_d << " x " << ny_d << " x " << nz_d << "\n";
    std::cout << "DICOM spacing   : " << sx_mm << " x " << sy_mm << " x " << sz_mm << " mm\n";

    // Physical bounding box of the DICOM volume
    // Origin (IPP of first slice) comes from ITK image metadata
    {
        const auto& org = huImage->GetOrigin();  // [mm], ITK reads from first slice IPP

        // Z of last slice: read Image Position Patient (0020|0032) from gdcmIO,
        // which holds the last-loaded slice's metadata after Update()
        double zLast = org[2] + (static_cast<double>(nz_d) - 1) * sz_mm; // fallback
        std::string ippStr;
        if (itk::ExposeMetaData<std::string>(gdcmIO->GetMetaDataDictionary(),
                                             "0020|0032", ippStr) && !ippStr.empty())
        {
            std::replace(ippStr.begin(), ippStr.end(), '\\', ' ');
            std::istringstream ss(ippStr);
            double ix, iy, iz;
            if (ss >> ix >> iy >> iz) zLast = iz;
        }

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "DICOM phys bbox :\n";
        std::cout << "  X : [" << org[0] << ", " << org[0] + nx_d*sx_mm << "] mm  ("
                  << nx_d*sx_mm << " mm)\n";
        std::cout << "  Y : [" << org[1] << ", " << org[1] + ny_d*sy_mm << "] mm  ("
                  << ny_d*sy_mm << " mm)\n";
        std::cout << "  Z : [" << org[2] << ", " << zLast + sz_mm << "] mm  ("
                  << (zLast + sz_mm - org[2]) << " mm)  [IPP first/last slice]\n";
    }

    // -------------------------------------------------------------------------
    // 3. Compute output volume geometry
    // -------------------------------------------------------------------------
    const double out_vxy = (vol_vxy_mm > 0.0) ? vol_vxy_mm : sx_mm;
    const double out_vz  = (vol_vz_mm  > 0.0) ? vol_vz_mm  : sz_mm;

    size_t out_nx, out_ny, out_nz;
    if (vol_width_mm > 0.0 && vol_height_mm > 0.0 && vol_length_mm > 0.0) {
        out_nx = static_cast<size_t>(std::ceil(vol_width_mm  / out_vxy));
        out_ny = static_cast<size_t>(std::ceil(vol_height_mm / out_vxy));
        out_nz = static_cast<size_t>(std::ceil(vol_length_mm / out_vz));
    } else {
        // Match DICOM physical extent at the requested spacing
        out_nx = static_cast<size_t>(std::ceil(nx_d * sx_mm / out_vxy));
        out_ny = static_cast<size_t>(std::ceil(ny_d * sy_mm / out_vxy));
        out_nz = static_cast<size_t>(std::ceil(nz_d * sz_mm / out_vz));
    }

    // Default DICOM corner: centre DICOM in output volume
    if (!dicom_corner_specified) {
        dicom_corner_x_mm = -(static_cast<double>(nx_d) * sx_mm) / 2.0;
        dicom_corner_y_mm = -(static_cast<double>(ny_d) * sy_mm) / 2.0;
        dicom_corner_z_mm = -(static_cast<double>(nz_d) * sz_mm) / 2.0;
    }

    // DICOM centre in DICOM-local space (mm from DICOM [0,0,0])
    const double qcx = (static_cast<double>(nx_d) * sx_mm) / 2.0;
    const double qcy = (static_cast<double>(ny_d) * sy_mm) / 2.0;
    const double qcz = (static_cast<double>(nz_d) * sz_mm) / 2.0;

    // Rotation matrix R (forward: DICOM-local → output isocenter offset)
    // R = Rx * Ry * Rz  (intrinsic XYZ Euler)
    const double deg2rad = std::acos(-1.0) / 180.0;
    const Mat3 R  = rotX(dicom_rot_x_deg*deg2rad) *
                    rotY(dicom_rot_y_deg*deg2rad) *
                    rotZ(dicom_rot_z_deg*deg2rad);
    const Mat3 Rt = R.T();  // inverse rotation

    std::cout << "Output volume   : " << out_nx << " x " << out_ny << " x " << out_nz << "\n";
    std::cout << "Output spacing  : " << out_vxy << " x " << out_vxy << " x " << out_vz << " mm\n";
    std::cout << "DICOM corner    : (" << dicom_corner_x_mm << ", "
              << dicom_corner_y_mm << ", " << dicom_corner_z_mm << ") mm (isocenter)\n";
    std::cout << "DICOM rotation  : (" << dicom_rot_x_deg << ", "
              << dicom_rot_y_deg << ", " << dicom_rot_z_deg << ") deg\n";

    // =========================================================================
    // 4. Pre-compute 2D masks (same for every Z slice → compute once)
    // =========================================================================
    const size_t slicePx = out_nx * out_ny;

    // ── Stretcher 2D mask: 0=outside, 10=carbon, 11=foam ─────────────────────
    std::vector<uint8_t> strMask;
    size_t strCarbCount = 0, strFoamCount = 0;
    if (stretcher_enable)
    {
        const double S_TOP_W    = 440.0;
        const double S_BOT_W    = 396.0;
        const double S_H        =  57.0;
        const double S_STR_H    =  20.0;
        const double S_WALL_T   =   1.5;
        const double S_CHAM     =   4.0;
        const double S_CORNER_R =   5.0;

        const double hw_t = S_TOP_W/2.0, hw_b = S_BOT_W/2.0;
        const double ht = S_H/2.0, ky = ht-S_STR_H, c = S_CHAM;

        // Convert corner-relative top-edge → isocenter polygon centre.
        // top_x/top_y point to the top edge of the stretcher as seen in the
        // DICOM viewer (minimum-Y side of the cross-section, i.e. the underside
        // of the physical stretcher), measured from the min-X / min-Y corner.
        const double str_cx = stretcher.top_x_mm - out_nx*out_vxy/2.0;
        const double str_cy = stretcher.top_y_mm - out_ny*out_vxy/2.0 + ht;

        const Poly2D sharpOuter = {
            {-hw_b,-ht},{hw_b,-ht},{hw_t,ky},{hw_t,ht-c},
            {hw_t-c,ht},{-hw_t+c,ht},{-hw_t,ht-c},{-hw_t,ky},
        };
        const Poly2D outerPoly = roundPolygon(sharpOuter, S_CORNER_R);
        const Poly2D innerPoly = roundPolygon(inwardOffset(sharpOuter, S_WALL_T),
                                              std::max(S_CORNER_R-S_WALL_T, 0.0));

        strMask.assign(slicePx, 0);
        for (size_t oj = 0; oj < out_ny; ++oj) {
            const double dy = str_cy
                            - ((oj+0.5)*out_vxy - out_ny*out_vxy*0.5);
            if (dy < -ht || dy > ht) continue;
            for (size_t oi = 0; oi < out_nx; ++oi) {
                const double dx = (oi+0.5)*out_vxy - out_nx*out_vxy*0.5
                                  - str_cx;
                if (dx < -hw_t || dx > hw_t) continue;
                if (!inConvex(outerPoly, dx, dy)) continue;
                strMask[oj*out_nx+oi] = inConvex(innerPoly, dx, dy) ? 11 : 10;
            }
        }
        std::cout << "Stretcher 2D mask computed.\n";
    }

    // ── Crop cylinder 2D mask: true = keep ───────────────────────────────────
    std::vector<bool> cropMask;
    if (crop_cylinder_enable)
    {
        const double r2 = crop_cylinder_radius_mm * crop_cylinder_radius_mm;
        cropMask.resize(slicePx);
        for (size_t oj = 0; oj < out_ny; ++oj) {
            const double dy = (oj+0.5)*out_vxy - out_ny*out_vxy*0.5
                              - crop_cylinder_cy_mm;
            for (size_t oi = 0; oi < out_nx; ++oi) {
                const double dx = (oi+0.5)*out_vxy - out_nx*out_vxy*0.5
                                  - crop_cylinder_cx_mm;
                cropMask[oj*out_nx+oi] = (dx*dx+dy*dy <= r2);
            }
        }
        std::cout << "Crop cylinder 2D mask computed.\n";
    }

    // ── NRRD segmentation mask ────────────────────────────────────────────────
    using MaskImageType = itk::Image<uint8_t, 3>;
    MaskImageType::Pointer maskImage;
    const uint8_t* maskBuf = nullptr;
    int mnx = 0, mny = 0, mnz = 0;
    double mo_x = 0, mo_y = 0, mo_z = 0;
    double ms_x = 1, ms_y = 1, ms_z = 1;

    if (!mask_nrrd_path.empty())
    {
        auto maskReader = itk::ImageFileReader<MaskImageType>::New();
        maskReader->SetFileName(mask_nrrd_path);
        maskReader->Update();
        maskImage = maskReader->GetOutput();
        maskImage->DisconnectPipeline();

        const auto& msz  = maskImage->GetLargestPossibleRegion().GetSize();
        const auto& msp  = maskImage->GetSpacing();
        const auto& morg = maskImage->GetOrigin();

        mnx = static_cast<int>(msz[0]);
        mny = static_cast<int>(msz[1]);
        mnz = static_cast<int>(msz[2]);
        ms_x = msp[0]; ms_y = msp[1]; ms_z = msp[2];
        mo_x = morg[0]; mo_y = morg[1]; mo_z = morg[2];

        maskBuf = maskImage->GetBufferPointer();
        std::cout << "Mask loaded     : " << mask_nrrd_path << "\n";
        std::cout << "  size="<<mnx<<"x"<<mny<<"x"<<mnz
                  <<" spacing="<<ms_x<<"x"<<ms_y<<"x"<<ms_z<<" mm\n";
    }

    // =========================================================================
    // 5. Build output paths and open .raw file for streaming write
    // =========================================================================
    const std::string dimTag = std::to_string(out_nx)+"x"+
                               std::to_string(out_ny)+"x"+
                               std::to_string(out_nz);
    const fs::path   prefixPath  = fs::path(outputPrefix);
    const std::string prefixStem = prefixPath.filename().string();
    const fs::path outDirPath = (fs::path(dicomDir).parent_path() /
                                 prefixPath.parent_path() /
                                 (prefixStem+"_vox_"+dimTag)).lexically_normal();
    fs::create_directories(outDirPath);
    const std::string outDir  = outDirPath.string()+"/";
    const std::string outBase = outDir+prefixStem+"_vox_";
    std::cout << "Output folder   : " << outDir << "\n";

    if (write_dicom)
        dicom_output_dir = outDirPath.string() + "_dicom";

    const bool hasImplant = !implants.empty();
    const std::string labelTag =
        std::string(hasImplant          ? "implant_"       : "") +
        (stretcher_enable               ? "stretcher_"     : "") +
        (crop_cylinder_enable           ? "reconCylinder_" : "") +
        (hasImplant || stretcher_enable ? "labels_"        : "5labels_");

    const std::string rawBase = outBase+labelTag+dimTag;
    const std::string rawPath = rawBase+".raw";

    std::ofstream rawFile(rawPath, std::ios::binary);
    if (!rawFile) throw std::runtime_error("Cannot open label file for writing: "+rawPath);

    // ── DICOM output: GDCM-based writer (full tag control, correct IPP Z) ─────
    const bool writeDicom = !dicom_output_dir.empty();
    // IPP = centre of first voxel [0,0,ok]
    const double ipp_x  = -(static_cast<double>(out_nx - 1) * out_vxy) / 2.0;
    const double ipp_y  = -(static_cast<double>(out_ny - 1) * out_vxy) / 2.0;
    const double ipp_z0 = -(static_cast<double>(out_nz - 1) * out_vz ) / 2.0;

    // Helper: set / replace a DICOM string tag in a DataSet
    auto gTag = [](gdcm::DataSet& ds, uint16_t g, uint16_t e, std::string v) {
        if (v.size() % 2 != 0) v += ' ';
        gdcm::DataElement de(gdcm::Tag(g, e));
        de.SetByteValue(v.c_str(), static_cast<uint32_t>(v.size()));
        ds.Replace(de);
    };

    // Persistent GDCM writer (set up once, updated per-slice to avoid copy churn)
    gdcm::ImageWriter gdcmWriter;
    gdcm::UIDGenerator gdcmUID;
    std::string newSeriesUID;
    if (writeDicom)
    {
        fs::create_directories(dicom_output_dir);
        gdcm::ImageReader tmpl;
        tmpl.SetFileName(fileNames[0].c_str());
        if (!tmpl.Read())
            throw std::runtime_error("Cannot read DICOM template for output: " + fileNames[0]);
        gdcmWriter.SetFile(tmpl.GetFile());   // copy template metadata once
        newSeriesUID = gdcmUID.Generate();

        // Fixed geometry (same every slice)
        gdcm::Image& gImg = gdcmWriter.GetImage();
        gImg.SetNumberOfDimensions(2);
        gImg.SetDimension(0, static_cast<unsigned int>(out_nx));
        gImg.SetDimension(1, static_cast<unsigned int>(out_ny));
        gdcm::PixelFormat pf(gdcm::PixelFormat::INT16);
        gImg.SetPixelFormat(pf);
        gImg.SetPhotometricInterpretation(gdcm::PhotometricInterpretation::MONOCHROME2);
        const double dircos[6] = {1,0,0, 0,1,0};
        gImg.SetDirectionCosines(dircos);
        const double spacing3[3] = {out_vxy, out_vxy, out_vz};
        gImg.SetSpacing(spacing3);
        gImg.SetIntercept(0.0);
        gImg.SetSlope(1.0);

        // Fixed metadata tags
        gdcm::DataSet& ds = gdcmWriter.GetFile().GetDataSet();
        gTag(ds, 0x0020, 0x000e, newSeriesUID);  // Series Instance UID
        gTag(ds, 0x0028, 0x1053, "1");            // Rescale Slope
        gTag(ds, 0x0028, 0x1052, "0");            // Rescale Intercept

        std::cout << "Writing DICOM output to: " << dicom_output_dir << "\n";
    }

    // =========================================================================
    // 6. Main slice loop: fill → apply overlays → write
    //    Only two slices (label + HU) are in RAM at any time.
    // =========================================================================
    std::vector<uint8_t> labelSlice(slicePx);
    std::vector<int16_t> huSlice(slicePx);

    const int16_t* dp  = huImage->GetBufferPointer();
    const int inx = static_cast<int>(nx_d);
    const int iny = static_cast<int>(ny_d);
    const int inz = static_cast<int>(nz_d);

    // Per-implant precomputed values
    struct ImpCache { double r2, halfH; };
    std::vector<ImpCache> impCache;
    for (const auto& imp : implants)
        impCache.push_back({imp.radius_mm*imp.radius_mm, imp.height_mm/2.0});
    std::vector<size_t> implantVoxCount(implants.size(), 0);

    size_t counts[12] = {0};
    size_t dicomVoxTotal = 0;
    size_t cropZeroTotal = 0;

    for (size_t ok = 0; ok < out_nz; ++ok)
    {
        const double pz = (ok+0.5)*out_vz - out_nz*out_vz*0.5;

        // ── Fill slice from DICOM (trilinear interp) ──────────────────────────
        std::fill(labelSlice.begin(), labelSlice.end(), uint8_t(0));
        std::fill(huSlice.begin(),    huSlice.end(),    int16_t(-1000));

        for (size_t oj = 0; oj < out_ny; ++oj) {
            const double py = (oj+0.5)*out_vxy - out_ny*out_vxy*0.5;
            for (size_t oi = 0; oi < out_nx; ++oi) {
                const double px = (oi+0.5)*out_vxy - out_nx*out_vxy*0.5;

                const double rx = px-dicom_corner_x_mm-qcx;
                const double ry = py-dicom_corner_y_mm-qcy;
                const double rz = pz-dicom_corner_z_mm-qcz;
                const auto [qrx,qry,qrz] = Rt.apply(rx,ry,rz);
                const double qx=qrx+qcx, qy=qry+qcy, qz=qrz+qcz;

                const double ci = qx/sx_mm-0.5;
                const double cj = qy/sy_mm-0.5;
                const double ck = qz/sz_mm-0.5;

                if (ci<-0.5||ci>inx-0.5||cj<-0.5||cj>iny-0.5||ck<-0.5||ck>inz-0.5)
                    continue;

                // ── NRRD mask: nearest-neighbour lookup in DICOM physical space ──
                if (maskBuf) {
                    const int mi = static_cast<int>(std::round((dcm_orig_x + ci*sx_mm - mo_x) / ms_x));
                    const int mj = static_cast<int>(std::round((dcm_orig_y + cj*sy_mm - mo_y) / ms_y));
                    const int mk = static_cast<int>(std::round((dcm_orig_z + ck*sz_mm - mo_z) / ms_z));
                    if (mi < 0 || mi >= mnx || mj < 0 || mj >= mny || mk < 0 || mk >= mnz ||
                        maskBuf[mk * mny * mnx + mj * mnx + mi] == 0)
                        continue;  // leave voxel as air
                }

                const double cic=std::max(0.0,std::min((double)(inx-1),ci));
                const double cjc=std::max(0.0,std::min((double)(iny-1),cj));
                const double ckc=std::max(0.0,std::min((double)(inz-1),ck));
                const int i0=(int)cic,i1=std::min(i0+1,inx-1);
                const int j0=(int)cjc,j1=std::min(j0+1,iny-1);
                const int k0=(int)ckc,k1=std::min(k0+1,inz-1);
                const double ti=cic-i0,tj=cjc-j0,tk=ckc-k0;

                auto huAt=[&](int ii,int jj,int kk)->double{
                    return (double)dp[kk*iny*inx+jj*inx+ii];};
                const double hu=
                    (1-tk)*((1-tj)*((1-ti)*huAt(i0,j0,k0)+ti*huAt(i1,j0,k0))
                          +    tj *((1-ti)*huAt(i0,j1,k0)+ti*huAt(i1,j1,k0)))
                    +  tk *((1-tj)*((1-ti)*huAt(i0,j0,k1)+ti*huAt(i1,j0,k1))
                          +    tj *((1-ti)*huAt(i0,j1,k1)+ti*huAt(i1,j1,k1)));

                const size_t ij = oj*out_nx+oi;
                const int16_t huVal = static_cast<int16_t>(
                    std::round(std::clamp(hu,-32768.0,32767.0)));
                huSlice[ij] = huVal;

                if      (huVal<thr_air_fat)        labelSlice[ij]=0;
                else if (huVal<thr_fat_soft)       labelSlice[ij]=1;
                else if (huVal<thr_soft_spongiosa) labelSlice[ij]=2;
                else if (huVal<thr_spongiosa_cort) labelSlice[ij]=3;
                else                               labelSlice[ij]=4;
                ++dicomVoxTotal;
            }
        }

        // ── Implant cylinders ─────────────────────────────────────────────────
        for (size_t impIdx = 0; impIdx < implants.size(); ++impIdx) {
            const auto& imp = implants[impIdx];
            if (std::abs(pz-imp.cz_mm) > impCache[impIdx].halfH) continue;
            const double r2 = impCache[impIdx].r2;
            for (size_t oj = 0; oj < out_ny; ++oj) {
                const double dy=(oj+0.5)*out_vxy-out_ny*out_vxy*0.5-imp.cy_mm;
                for (size_t oi = 0; oi < out_nx; ++oi) {
                    const double dx=(oi+0.5)*out_vxy-out_nx*out_vxy*0.5-imp.cx_mm;
                    if (dx*dx+dy*dy<=r2) {
                        const size_t ij=oj*out_nx+oi;
                        labelSlice[ij]=5; huSlice[ij]=3000;
                        ++implantVoxCount[impIdx];
                    }
                }
            }
        }

        // ── Crop cylinder ─────────────────────────────────────────────────────
        if (crop_cylinder_enable) {
            for (size_t ij = 0; ij < slicePx; ++ij)
                if (!cropMask[ij]) {
                    labelSlice[ij]=0; huSlice[ij]=-1000; ++cropZeroTotal;
                }
        }

        // ── Stretcher ─────────────────────────────────────────────────────────
        if (stretcher_enable) {
            for (size_t ij = 0; ij < slicePx; ++ij) {
                const uint8_t m = strMask[ij];
                if (m==10) { labelSlice[ij]=10; huSlice[ij]=3000; ++strCarbCount; }
                else if (m==11) { labelSlice[ij]=11; huSlice[ij]=-100; ++strFoamCount; }
            }
        }

        // ── Label counts ──────────────────────────────────────────────────────
        for (uint8_t v : labelSlice) if (v<12) ++counts[v];

        // ── Write to .raw ─────────────────────────────────────────────────────
        rawFile.write(reinterpret_cast<const char*>(labelSlice.data()),
                      static_cast<std::streamsize>(slicePx));

        // ── Write DICOM slice (GDCM persistent writer, correct IPP + pixel spacing)
        if (writeDicom) {
            gdcm::Image&   gImg = gdcmWriter.GetImage();
            gdcm::DataSet& ds   = gdcmWriter.GetFile().GetDataSet();

            // Per-slice geometry
            const double origin3[3] = {ipp_x, ipp_y, ipp_z0 + ok * out_vz};
            gImg.SetOrigin(origin3);

            // Per-slice metadata
            gTag(ds, 0x0020, 0x0013, std::to_string(ok + 1));  // Instance Number
            gTag(ds, 0x0008, 0x0018, gdcmUID.Generate());       // SOP Instance UID (unique)

            // Pixel data (replaces previous slice)
            gdcm::DataElement pixDE(gdcm::Tag(0x7fe0, 0x0010));
            pixDE.SetByteValue(
                reinterpret_cast<const char*>(huSlice.data()),
                static_cast<uint32_t>(out_nx * out_ny * sizeof(int16_t)));
            gImg.SetDataElement(pixDE);

            std::ostringstream fname;
            fname << dicom_output_dir << "/CT."
                  << std::setfill('0') << std::setw(4) << (ok + 1) << ".dcm";
            gdcmWriter.SetFileName(fname.str().c_str());
            if (!gdcmWriter.Write())
                throw std::runtime_error("DICOM write failed: " + fname.str());
        }

        if ((ok+1)%100==0 || ok+1==out_nz)
            std::cout << "  slice "<<(ok+1)<<"/"<<out_nz<<"\n"<<std::flush;
    }

    rawFile.close();
    if (writeDicom)
        std::cout << "DICOM output written: " << dicom_output_dir << "\n";

    // ── Post-loop summaries ───────────────────────────────────────────────────
    std::cout << "DICOM placement : " << dicomVoxTotal << " voxels filled.\n";
    for (size_t i = 0; i < implants.size(); ++i)
        std::cout << "Implant[" << i << "]: centre=("
                  << implants[i].cx_mm<<","<<implants[i].cy_mm<<","<<implants[i].cz_mm
                  <<") r="<<implants[i].radius_mm<<" h="<<implants[i].height_mm
                  <<" -> "<<implantVoxCount[i]<<" voxels\n";
    if (crop_cylinder_enable)
        std::cout << "Crop cylinder  : "<<cropZeroTotal<<" voxels zeroed.\n";
    if (stretcher_enable)
        std::cout << "Stretcher shell: "<<strCarbCount<<" carbon + "<<strFoamCount<<" foam voxels.\n";

    {
        const char* names[12]={"air","fat","soft","spongiosa","cortical","implant",
                                "","","","","carbon","foam"};
        const size_t totalVox = out_nx*out_ny*out_nz;
        std::cout<<std::fixed<<std::setprecision(2)<<"Label distribution:\n";
        for (int l=0;l<12;++l)
            if (counts[l]>0 && names[l][0]!='\0')
                std::cout<<"  "<<l<<" ("<<names[l]<<"): "<<counts[l]
                         <<"  ("<<100.0*counts[l]/totalVox<<" %)\n";
    }

    std::cout << "Label phantom written: " << rawPath << "\n";
    writeInfoFile(rawBase,
                  out_nx, out_ny, out_nz,
                  out_vxy/10.0, out_vxy/10.0, out_vz/10.0,
                  shift_x_mm/10.0, shift_y_mm/10.0, shift_z_mm/10.0);

    // -------------------------------------------------------------------------
    // 12. Write MC-GPU .in file
    // -------------------------------------------------------------------------
    if (!mcgpu_in_template.empty())
    {
        std::ifstream tmpl(mcgpu_in_template);
        if (!tmpl) {
            std::cerr << "Warning: cannot open MC-GPU template: " << mcgpu_in_template << "\n";
        } else {
            const std::string inPath = outDir + "CBCT.in";
            std::ofstream out(inPath);
            if (!out) throw std::runtime_error("Cannot write .in file: " + inPath);

            const double sx_cm  = out_vxy/10.0, sy_cm = out_vxy/10.0, sz_cm = out_vz/10.0;
            const double off_x  = -(out_nx*sx_cm/2.0) + shift_x_mm/10.0;
            const double off_y  = -(out_ny*sy_cm/2.0) + shift_y_mm/10.0;
            const double off_z  = -(out_nz*sz_cm/2.0) + shift_z_mm/10.0;
            const std::string absRawPath = fs::absolute(rawPath).string();

            std::string line;
            int skipLines = 0;
            while (std::getline(tmpl, line)) {
                if (line.find("#[SECTION IMAGE DETECTOR") != std::string::npos) {
                    out << line << "\n";
                    out << mcgpu_output_name << "   # OUTPUT IMAGE FILE NAME\n";
                    out << mcgpu_det_nx << "      " << mcgpu_det_nz
                        << "                  # NUMBER OF PIXELS IN THE IMAGE: Nx Nz\n";
                    skipLines = 2;
                } else if (line.find("#[SECTION VOXELIZED GEOMETRY FILE") != std::string::npos) {
                    out << line << "\n";
                    out << std::fixed << std::setprecision(3);
                    out << absRawPath << "     # VOXEL GEOMETRY FILE\n";
                    out << " " << off_x << "  " << off_y << "  " << off_z
                        << "              # OFFSET OF THE VOXEL GEOMETRY [cm]\n";
                    out << " " << out_nx << " " << out_ny << " " << out_nz
                        << "                 # NUMBER OF VOXELS\n";
                    out << " " << sx_cm << " " << sy_cm << " " << sz_cm
                        << "           # VOXEL SIZES [cm]\n";
                    out << " 0 0 0                          # SIZE OF LOW RESOLUTION VOXELS\n";
                    skipLines = 5;
                } else if (skipLines > 0) {
                    --skipLines;
                } else {
                    out << line << "\n";
                }
            }
            std::cout << "MC-GPU .in file written: " << inPath << "\n";
        }
    }

    std::cout << "Done.\n";
    return 0;
}
