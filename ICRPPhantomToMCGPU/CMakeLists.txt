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

# -----------------------------------------------------------------------------
# 2. Project Settings
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include the 'include' folder so we can use #include "G4DatReader.hpp"
include_directories(include)

# -----------------------------------------------------------------------------
# 3. Shared Library (The logic used by both EXEs)
# -----------------------------------------------------------------------------
# This avoids compiling the same code twice for each executable
add_library(ConverterLogic 
    src/G4DatReader.cpp 
    src/ImageUtils.cpp
)
target_link_libraries(ConverterLogic ${ITK_LIBRARIES})

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
add_executable(raw_converter src/raw_converter.cpp)
target_link_libraries(raw_converter ConverterLogic)

# EXE 2: preprocessor (HU Map + Resample + Padding)
add_executable(preprocessor src/dicom_fitter.cpp)
target_link_libraries(preprocessor ConverterLogic)