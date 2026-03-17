#pragma once
#include <string>
#include <itkImage.h>

// ---------------------------------------------------------------------------
// HUDicomExporter
// Writes an int16 HU volume as a DICOM CT series (one file per axial slice).
// Stored pixel = HU + 1024  (uint16, RescaleIntercept=-1024, Slope=1)
// ---------------------------------------------------------------------------
class HUDicomExporter
{
public:
    using HUImageType = itk::Image<int16_t, 3>;

    struct Options
    {
        std::string patientName       = "MRCP_Phantom";
        std::string seriesDescription = "HU Phantom";
        double      windowCenter      =   40.0;  // [HU]
        double      windowWidth       =  400.0;  // [HU]
    };

    // outputDir is created if it does not exist.
    static void Write(HUImageType::Pointer   huImage,
                      const std::string&     outputDir,
                      const Options&         opts);

    static void Write(HUImageType::Pointer   huImage,
                      const std::string&     outputDir)
    { Write(huImage, outputDir, Options{}); }
};
