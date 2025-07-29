#----------------------------------------------------------------------------
# Setup the project
cmake_minimum_required(VERSION 3.16...3.27)
project(cbctsim)

#----------------------------------------------------------------------------
# Find Geant4 package, activating all available UI and Vis drivers by default
# You can set WITH_GEANT4_UIVIS to OFF via the command line or ccmake/cmake-gui
# to build a batch mode only executable
#
option(WITH_GEANT4_UIVIS "Build example with Geant4 UI and Vis drivers" ON)
if(WITH_GEANT4_UIVIS)
  find_package(Geant4 REQUIRED ui_all vis_all)
else()
  find_package(Geant4 REQUIRED)
endif()



# include(ExternalProject)
# ExternalProject_Add(ICRPphantoms
#   SOURCE_DIR ../../ICRP110_HumanPhantoms/
#   CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external-ICRP
#   UPDATE_COMMAND "" INSTALL_COMMAND ""
# )

# ExternalProject_Get_Property(ICRPphantoms BINARY_DIR)






#----------------------------------------------------------------------------
# Setup Geant4 include directories and compile definitions
#
include(${Geant4_USE_FILE})

#----------------------------------------------------------------------------
# Locate sources and headers for this project
#
include_directories(${PROJECT_SOURCE_DIR}/include 
                    ${PROJECT_SOURCE_DIR}/ICRPinclude
                    ${Geant4_INCLUDE_DIR})

file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc 
                  ${PROJECT_SOURCE_DIR}/ICRPsrc/*.cc )
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hh
                  ${PROJECT_SOURCE_DIR}/ICRPinclude/*.hh )


include(ExternalProject)
ExternalProject_Add(ICRPdata
  SOURCE_DIR ${PROJECT_BINARY_DIR}/ICRPdata
  URL https://cern.ch/geant4-data/datasets/examples/advanced/ICRP110Phantoms/ICRPdata.tar.gz
  URL_MD5 2aba5409d4e7b7a2f0328e7d3402f187
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  )

#----------------------------------------------------------------------------
# Add the executable, and link it to the Geant4 libraries
#
add_executable(particleGun particleGun.cc ${sources} ${headers})
add_executable(ICRP110phantoms ICRP110phantoms.cc ${sources} ${headers})

target_link_libraries(particleGun ${Geant4_LIBRARIES} )
target_link_libraries(ICRP110phantoms ${Geant4_LIBRARIES} )


add_dependencies(ICRP110phantoms ICRPdata)

# target_link_libraries(particleGun PRIVATE
#   "${BINARY_DIR}/$<CONFIG>/libICRPphantoms.a"
#   ${Geant4_LIBRARIES}
# )
#----------------------------------------------------------------------------
# Copy all scripts to the build directory, i.e. the directory in which we
# build particleGun. This is so that we can run the executable directly because it
# relies on these scripts being in the current working directory.
#
set(particleGun_SCRIPTS
female.in female_head.in female_trunk.in male.in male_head.in male_trunk.in ColourMap.dat g4views/g4_00.g4view g4views/g4_01.g4view
    plotHisto.C run.mac vis.mac
    geo.mac IPEMXraysCatalogue.txt muspectrum.mac
    G4_Al.txt G4_BRASS.txt G4_CESIUM_IODIDE.txt 
  )

foreach(_script ${particleGun_SCRIPTS})
  configure_file(
    ${PROJECT_SOURCE_DIR}/${_script}
    ${PROJECT_BINARY_DIR}/${_script}
    COPYONLY
    )
endforeach()

#----------------------------------------------------------------------------
# Install the executable to 'bin' directory under CMAKE_INSTALL_PREFIX
#
#install(TARGETS particleGun DESTINATION bin)
