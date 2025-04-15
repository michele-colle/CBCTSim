#----------------------------------------------------------------------------
# Setup the project
#
cmake_minimum_required(VERSION 3.16...3.27)
project(geant4cbctsimmik)

#----------------------------------------------------------------------------
# Find Geant4 package, activating all available UI and Vis drivers by default
# See the documentation for a guide on how to enable/disable specific components
#
find_package(Geant4 REQUIRED ui_all vis_all)

#----------------------------------------------------------------------------
# Locate sources and headers for this project
# NB: headers are included so they will show up in IDEs
#
file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hh)

#----------------------------------------------------------------------------
# Add the executable, use our local headers, and link it to the Geant4 libraries
#
add_executable(ctsim main.cc ${sources} ${headers})
target_include_directories(ctsim PRIVATE include)
target_link_libraries(ctsim PRIVATE ${Geant4_LIBRARIES})

add_custom_target(geant4cbctsimmik DEPENDS ctsim)
#----------------------------------------------------------------------------
# Copy all scripts to the build directory, i.e. the directory in which we
# build B1. This is so that we can run the executable directly because it
# relies on these scripts being in the current working directory.
#
# set(EXAMPLEB1_SCRIPTS
  # init_vis.mac
  # )

# foreach(_script ${EXAMPLEB1_SCRIPTS})
  # configure_file(
    # ${PROJECT_SOURCE_DIR}/${_script}
    # ${PROJECT_BINARY_DIR}/${_script}
    # COPYONLY
    # )
# endforeach()
