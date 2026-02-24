#include <cmath>
#include <cfloat>
#include <iostream>
#include <string>

#include "TETModelImport.hh"
#include "G4DatReader.hpp"
#include "ImageUtils.hpp"

#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Tet.hh"
#include "G4UIExecutive.hh"

int main(int argc, char** argv)
{
    // ---------------------------------------------------------------------
    // 1. Parse command line arguments
    // ---------------------------------------------------------------------
    std::string phantomName = "MRCP-00F";
    double voxelSize_mm = 0.2;
    std::string outputBase = "./output/MRCP_00F_vox_";

    if (argc > 1) {
        phantomName = argv[1];
        outputBase = "./output/" + phantomName + "_vox_";
    }
    if (argc > 2) {
        voxelSize_mm = std::stod(argv[2]);
    }

    std::cout << "Using phantom: " << phantomName << std::endl;
    std::cout << "Target voxel size: " << voxelSize_mm << " mm" << std::endl;

    // ---------------------------------------------------------------------
    // 2. Load tetrahedral phantom (MRCP) via TETModelImport
    // ---------------------------------------------------------------------
    G4UIExecutive* ui = nullptr;
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

    std::cout << "Bounding box size [mm]: "
              << lenX << " x " << lenY << " x " << lenZ << std::endl;
    std::cout << "Voxel grid size: "
              << nx << " x " << ny << " x " << nz << std::endl;

    if (nx <= 0 || ny <= 0 || nz <= 0) {
        std::cerr << "Invalid voxel grid size. Check bounding box or voxel size." << std::endl;
        return 1;
    }

    // ---------------------------------------------------------------------
    // 3. Allocate ITK label image (unsigned short) for material IDs
    // ---------------------------------------------------------------------
    using LabelImageType = G4DatReader::LabelImageType;

    LabelImageType::Pointer image = LabelImageType::New();
    LabelImageType::SizeType size;
    size[0] = static_cast<LabelImageType::SizeType::SizeValueType>(nx);
    size[1] = static_cast<LabelImageType::SizeType::SizeValueType>(ny);
    size[2] = static_cast<LabelImageType::SizeType::SizeValueType>(nz);

    LabelImageType::RegionType region;
    region.SetSize(size);

    image->SetRegions(region);
    image->Allocate();
    image->FillBuffer(0); // default to Air / background

    double spacing[3] = { voxelSize_mm, voxelSize_mm, voxelSize_mm };
    image->SetSpacing(spacing);

    double origin[3] = { 0.0, 0.0, 0.0 };
    image->SetOrigin(origin);

    image->SetDirection(LabelImageType::DirectionType::GetIdentity());

    // ---------------------------------------------------------------------
    // 4. Voxelization loop using Geant4 G4Tet::Inside
    // ---------------------------------------------------------------------
    const G4int numTet = tetImport.GetNumTetrahedron();
    std::cout << "Starting voxelization over " << numTet << " tetrahedra..." << std::endl;

    for (G4int t = 0; t < numTet; ++t) {
        if (t % 100000 == 0) {
            std::cout << "Processing tet " << t << " / " << numTet << std::endl;
        }

        G4Tet* tetSolid = tetImport.GetTetrahedron(t);
        const std::vector<G4ThreeVector>& vertices = tetSolid->GetVertices();

        G4double tMinX(DBL_MAX), tMinY(DBL_MAX), tMinZ(DBL_MAX);
        G4double tMaxX(-DBL_MAX), tMaxY(-DBL_MAX), tMaxZ(-DBL_MAX);

        for (const auto& v : vertices) {
            if (v.x() < tMinX) tMinX = v.x();
            if (v.x() > tMaxX) tMaxX = v.x();
            if (v.y() < tMinY) tMinY = v.y();
            if (v.y() > tMaxY) tMaxY = v.y();
            if (v.z() < tMinZ) tMinZ = v.z();
            if (v.z() > tMaxZ) tMaxZ = v.z();
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

        for (G4int k = std::max(0, kMin); k <= std::min(kMax, nz - 1); ++k) {
            for (G4int j = std::max(0, jMin); j <= std::min(jMax, ny - 1); ++j) {
                for (G4int i = std::max(0, iMin); i <= std::min(iMax, nx - 1); ++i) {
                    const G4double x = bbMin.x() + (i + 0.5) * voxelSize;
                    const G4double y = bbMin.y() + (j + 0.5) * voxelSize;
                    const G4double z = bbMin.z() + (k + 0.5) * voxelSize;

                    G4ThreeVector pos(x, y, z);
                    if (tetSolid->Inside(pos) == kOutside) continue;

                    LabelImageType::IndexType idx;
                    idx[0] = static_cast<LabelImageType::IndexType::IndexValueType>(i);
                    idx[1] = static_cast<LabelImageType::IndexType::IndexValueType>(j);
                    idx[2] = static_cast<LabelImageType::IndexType::IndexValueType>(k);

                    image->SetPixel(idx, static_cast<unsigned short>(matID));
                }
            }
        }
    }

    std::cout << "Voxelization finished. Writing MCGPU RAW + info..." << std::endl;

    // ---------------------------------------------------------------------
    // 5. Save as MCGPU-compatible RAW + .txt using existing helper
    // ---------------------------------------------------------------------
    ImageUtils::LabelsToRawFile(image, outputBase);

    std::cout << "Done." << std::endl;
    return 0;
}
