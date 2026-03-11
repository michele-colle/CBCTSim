cmake_minimum_required(VERSION 3.10)
project(ICRP_Converter)

# -----------------------------------------------------------------------------
# 1. ITK Configuration
# -----------------------------------------------------------------------------
find_package(ITK REQUIRED COMPONENTS 
    ITKCommon 
    ITKIOImageBase 
    ITKIOGDCM 
    ITKIONRRD
    ITKTransform       
    ITKImageFunction
    ITKImageGrid        # Essential for Resampling/Padding
)
include(${ITK_USE_FILE})

find_package(Geant4 REQUIRED)
include(${Geant4_USE_FILE})

# -----------------------------------------------------------------------------
# 2. Project Settings
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include the 'include' folder so we can use #include "G4DatReader.hpp"
include_directories(include
                    MCRP_include
                    ${Geant4_INCLUDE_DIRS})

# -----------------------------------------------------------------------------
# 3. Shared Library (The logic used by both EXEs)
# -----------------------------------------------------------------------------
# This avoids compiling the same code twice for each executable

# 1. Collect all .cpp files in the src directory
file(GLOB CONVERTER_SOURCES "src/*.c*")

# 2. Create the library using that list
add_library(ConverterLogic ${CONVERTER_SOURCES})

# add_library(ConverterLogic 
#     src/G4DatReader.cpp 
#     src/ImageUtils.cpp
# )
target_link_libraries(ConverterLogic ${ITK_LIBRARIES} ${Geant4_LIBRARIES})

include(ExternalProject)
ExternalProject_Add(ICRPdata
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/data/ICRPdata
  URL https://cern.ch/geant4-data/datasets/examples/advanced/ICRP110Phantoms/ICRPdata.tar.gz
  URL_MD5 2aba5409d4e7b7a2f0328e7d3402f187
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  )

# -----------------------------------------------------------------------------
# 4. Executables
# -----------------------------------------------------------------------------

# EXE 1: raw_converter (g4dat -> Organ NRRD)
add_executable(raw_converter raw_converter.cpp)
target_link_libraries(raw_converter ConverterLogic)

# EXE 2: preprocessor (HU Map + Resample + Padding)
add_executable(dicom_fitter dicom_fitter.cpp)
target_link_libraries(dicom_fitter ConverterLogic)

# EXE 3: mcrp_to_vox (TET MRCP -> voxelized MCGPU phantom)
add_executable(mcrp_to_vox
    mcrp_to_vox.cpp
    MCRP_src/TETModelImport.cc
)
target_link_libraries(mcrp_to_vox ConverterLogic ${Geant4_LIBRARIES})

# EXE 4: mcrp_to_mcgpu_materials (TET MRCP -> Penelope .mat -> MC-GPU .mcgpu)
add_executable(mcrp_to_mcgpu_materials
    mcrp_to_mcgpu_materials.cpp
    MCRP_src/TETModelImport.cc
)
target_link_libraries(mcrp_to_mcgpu_materials ConverterLogic ${Geant4_LIBRARIES})