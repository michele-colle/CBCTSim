#include "HUDicomExporter.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <itkGDCMImageIO.h>
#include <itkImageSeriesWriter.h>
#include <itkMetaDataObject.h>
#include <itkNumericSeriesFileNames.h>

#include <gdcmDataElement.h>
#include <gdcmFile.h>
#include <gdcmFileExplicitFilter.h>
#include <gdcmReader.h>
#include <gdcmTag.h>
#include <gdcmVR.h>
#include <gdcmWriter.h>

// ---------------------------------------------------------------------------
// Minimal DICOM UID generator — 2.25.<timestamp+counter>, no GDCM header needed
// ---------------------------------------------------------------------------
static std::string generateUID()
{
    static std::atomic<uint64_t> counter{0};
    const uint64_t ns = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const uint64_t val = ns ^ (counter.fetch_add(1) * 6364136223846793005ULL);
    return "2.25." + std::to_string(val);
}

// ---------------------------------------------------------------------------
void HUDicomExporter::Write(HUImageType::Pointer   huImage,
                            const std::string&     outputDir,
                            const Options&         opts)
{
    namespace fs = std::filesystem;
    fs::create_directories(outputDir);

    // Write int16 HU values directly as signed 16-bit DICOM pixels.
    // RescaleIntercept=0 / RescaleSlope=1  →  displayed HU = stored pixel.
    using SliceType  = itk::Image<int16_t, 2>;
    using WriterType = itk::ImageSeriesWriter<HUImageType, SliceType>;

    const auto&  size    = huImage->GetLargestPossibleRegion().GetSize();
    const auto&  spacing = huImage->GetSpacing();
    const auto&  origin  = huImage->GetOrigin();
    const size_t nSlices = size[2];

    // ── File name list ───────────────────────────────────────────────────────
    auto nameGen = itk::NumericSeriesFileNames::New();
    nameGen->SetSeriesFormat(outputDir + "/slice_%04d.dcm");
    nameGen->SetStartIndex(0);
    nameGen->SetEndIndex(static_cast<itk::SizeValueType>(nSlices - 1));
    nameGen->SetIncrementIndex(1);

    // ── Shared UIDs ──────────────────────────────────────────────────────────
    const std::string studyUID  = generateUID();
    const std::string seriesUID = generateUID();
    const std::string frameUID  = generateUID();

    auto ds = [](double v, int prec = 6) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(prec) << v;
        return ss.str();
    };

    // ── Per-slice metadata ───────────────────────────────────────────────────
    std::vector<itk::MetaDataDictionary>  dicts(nSlices);
    std::vector<itk::MetaDataDictionary*> dictPtrs(nSlices);

    for (size_t k = 0; k < nSlices; ++k)
    {
        auto& dict = dicts[k];
        auto set = [&](const std::string& tag, const std::string& val) {
            itk::EncapsulateMetaData<std::string>(dict, tag, val);
        };

        // ── File meta ────────────────────────────────────────────────────────
        // 0002|0010 (TransferSyntaxUID) is set by FileExplicitFilter in the post-pass
        set("0002|0012", "2.25.1");                    // ImplementationClassUID (short, valid)
        set("0002|0013", "MRCP_CONV_1.0");             // ImplementationVersionName (≤16 chars)
        set("0002|0003", generateUID());               // MediaStorageSOPInstanceUID

        // ── Identifiers ──────────────────────────────────────────────────────
        set("0008|0060", "CT");
        set("0028|0004", "MONOCHROME2");               // PhotometricInterpretation
        set("0008|0070", "MRCP Converter");
        set("0008|103e", opts.seriesDescription);
        set("0010|0010", opts.patientName);
        set("0010|0020", opts.patientName);
        set("0020|000d", studyUID);
        set("0020|000e", seriesUID);
        set("0020|0052", frameUID);
        set("0008|0018", generateUID());               // SOPInstanceUID (per slice)

        // ── Geometry — centred FOV, Z from 0 [mm] ────────────────────────────
        set("0020|0013", std::to_string(k + 1));       // InstanceNumber
        set("0018|0050", ds(spacing[2]));              // SliceThickness
        set("0018|5100", "HFS");                       // PatientPosition
        set("0028|0030", ds(spacing[1]) + "\\" + ds(spacing[0])); // PixelSpacing
        set("0020|0037", "1\\0\\0\\0\\1\\0");          // ImageOrientationPatient (axial)

        // ImagePositionPatient: centre of first voxel of this slice.
        // X/Y centred on the FOV, Z runs from 0 → nSlices*dz [mm].
        const double imgX = -(static_cast<double>(size[0]) / 2.0) * spacing[0];
        const double imgY = -(static_cast<double>(size[1]) / 2.0) * spacing[1];
        const double imgZ =   static_cast<double>(k) * spacing[2];
        set("0020|0032", ds(imgX) + "\\" + ds(imgY) + "\\" + ds(imgZ));

        // ── Intensity ────────────────────────────────────────────────────────
        set("0028|1052", "0");                         // RescaleIntercept
        set("0028|1053", "1");                         // RescaleSlope
        set("0028|1054", "HU");                        // RescaleType

        set("0028|1050", ds(opts.windowCenter, 1));    // WindowCenter
        set("0028|1051", ds(opts.windowWidth,  1));    // WindowWidth

        dictPtrs[k] = &dicts[k];
    }

    // ── Write — signed 16-bit (PixelRepresentation=1) ────────────────────────
    auto gdcmIO = itk::GDCMImageIO::New();
    gdcmIO->SetPixelType(itk::IOPixelEnum::SCALAR);
    gdcmIO->SetComponentType(itk::IOComponentEnum::SHORT);

    auto writer = WriterType::New();
    writer->SetInput(huImage);
    writer->SetImageIO(gdcmIO);
    writer->SetFileNames(nameGen->GetFileNames());
    writer->SetMetaDataDictionaryArray(&dictPtrs);
    writer->Update();

    // ── Post-process: convert each file from Implicit VR LE → Explicit VR LE ──
    // ITK/GDCM writes Implicit VR LE by default; we use gdcm::FileExplicitFilter
    // to enforce 1.2.840.10008.1.2.1 (Explicit VR Little Endian) in-place.
    for (const auto& fname : nameGen->GetFileNames())
    {
        gdcm::Reader r;
        r.SetFileName(fname.c_str());
        if (!r.Read())
            throw std::runtime_error("GDCM: cannot re-read " + fname);

        gdcm::FileExplicitFilter fef;
        fef.SetFile(r.GetFile());
        fef.SetChangePrivateTags(false);
        fef.SetUseVRUN(true);
        if (!fef.Change())
            throw std::runtime_error("GDCM: FileExplicitFilter failed on " + fname);

        // FileExplicitFilter converts VRs but does NOT update the FMI TS tag —
        // set it explicitly so the on-disk header reflects 1.2.840.10008.1.2.1
        fef.GetFile().GetHeader().SetDataSetTransferSyntax(
            gdcm::TransferSyntax(gdcm::TransferSyntax::ExplicitVRLittleEndian));

        // Replace ImplementationClassUID (0002,0012) with a short valid UID
        // (GDCM auto-generates one that exceeds the 64-char DICOM limit)
        {
            static const char kImplUID[] = "2.25.1";
            gdcm::DataElement de(gdcm::Tag(0x0002, 0x0012));
            de.SetVR(gdcm::VR::UI);
            de.SetByteValue(kImplUID, static_cast<gdcm::VL>(strlen(kImplUID)));
            fef.GetFile().GetHeader().Replace(de);
        }

        gdcm::Writer w;
        w.SetFileName(fname.c_str());
        w.SetFile(fef.GetFile());
        if (!w.Write())
            throw std::runtime_error("GDCM: cannot write Explicit VR file " + fname);
    }

    std::cout << "DICOM series written to: " << outputDir
              << "  (" << nSlices << " slices, Explicit VR LE)" << std::endl;

    // ── Verify: read back the first slice and report its transfer syntax ──────
    {
        const std::string& firstFile = nameGen->GetFileNames().front();
        gdcm::Reader vr;
        vr.SetFileName(firstFile.c_str());
        if (vr.Read())
        {
            const gdcm::TransferSyntax& ts =
                vr.GetFile().GetHeader().GetDataSetTransferSyntax();
            std::cout << "  [verify] Transfer Syntax: "
                      << ts.GetString()                         // human-readable name
                      << "  (" << ts << ")"                    // UID
                      << (ts == gdcm::TransferSyntax::ExplicitVRLittleEndian
                          ? "  ✓" : "  ✗ NOT explicit VR LE!")
                      << std::endl;
        }
        else
        {
            std::cerr << "  [verify] Could not re-read " << firstFile << std::endl;
        }
    }
}
