============================================================
ICRP PHANTOM TO MCGPU PREPROCESSING PIPELINE
============================================================

1. VOI DEFINITION (3D SLICER)
-----------------------------
- Load the full ICRP NRRD volume into 3D Slicer.
- Use the 'Segment Editor' to define a Volume of Interest (VOI).
- Recommended: Use the 'Scissors' tool in 3D view to create a 
  bounding box or freeform mask around the target anatomy.
- Export the segment as a binary Labelmap (.nrrd): vai in data -> tasto destro sulla segmentazione -> export to labels
ensuring 
  'Compression' is turned OFF.
  -salva in data/ICRP_segmentation_data/<nome>.nrrd

2. C++ PREPROCESSING (RAW_CONVERTER)
------------------------------------
- The app loads the original ICRP .g4dat phantom data.
- It imports the Slicer VOI mask to determine cropping bounds.
- PIPELINE STEPS:
    a) MASKING: Sets all voxels outside the Slicer mask to 0 (Air).
    b) CROPPING: Trims the volume to the minimum bounding box 
       of the mask to reduce file size.
    c) MAPPING: Converts ICRP Organ IDs (16-bit) to simplified 
       MCGPU Material IDs (8-bit).
    d) CENTERING: Calculates the physical origin so the 
       geometry is centered at (0,0,0) in the simulation world.

3. OUTPUTS
----------
- [filename]_[dim].raw: 8-bit unsigned binary file containing 
  the material map.
- [filename].info: MCGPU-compatible header containing:
    - Offset (Lower-back corner in cm)
    - Voxel Count
    - Voxel Size (Spacing in cm)

4. NEXT STEPS (PYTHON)
----------------------
- Use 'itk-elastix' to register the generated .raw phantom 
  to the patient's CBCT DICOM.
- Apply the resulting transform to the phantom coordinates 
  for final Monte Carlo dose calculation.
============================================================


nota: ho usato questo per evitare che si creassero gli zone identfier:
https://stackoverflow.com/questions/4496697/what-are-zone-identifier-files-and-how-do-i-prevent-them-from-being-created