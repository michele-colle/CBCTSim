import itk

#https://proceedings.scipy.org/articles/gerudo-f2bc6f59-00d
# 1. Load the resampled volumes
fixed_image = itk.imread("output/python_dicom.nrrd", itk.F)
moving_image = itk.imread("output/python_hu_phantom.nrrd", itk.F)
label_image = itk.imread("output/python_labels.nrrd", itk.US)

transform_file = "output/TransformHeadFemale.h5"
transform = itk.transformread(transform_file)[0]

# 3. Resample the Labels
# We use NearestNeighbor to ensure organ IDs stay as integers (no blurring)
interpolator = itk.NearestNeighborInterpolateImageFunction.New(label_image)
resampled_labels = itk.resample_image_filter(
    label_image,
    transform=transform,
    interpolator=interpolator,
    use_reference_image=True,
    reference_image=label_image, # Keeps original size/spacing, or use the Patient DICOM
    default_pixel_value=0
)

interpolator = itk.NearestNeighborInterpolateImageFunction.New(moving_image)
resampled_image = itk.resample_image_filter(
    moving_image,
    transform=transform,
    interpolator=interpolator,
    use_reference_image=True,
    reference_image=moving_image, # Keeps original size/spacing, or use the Patient DICOM
    default_pixel_value=-1000.0
)

# 4. Save as NRRD for verification in Slicer
itk.imwrite(resampled_labels, "output/registered_labels.nrrd")
# 6. Save the registered HU phantom for visual check
itk.imwrite(resampled_image, "output/registered_hu_phantom.nrrd")

print("Rigid transformation applied.")