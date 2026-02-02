import itk

#https://proceedings.scipy.org/articles/gerudo-f2bc6f59-00d
# 1. Load the resampled volumes
fixed_image = itk.imread("output/python_dicom.nrrd", itk.F)
moving_image = itk.imread("output/python_hu_phantom.nrrd", itk.F)


# Configure a (default) parameter map with all the
# registration parameters
par_obj = itk.ParameterObject.New()
par_map = par_obj.GetDefaultParameterMap('rigid')
# Optimization Settings for speed and robustness
par_map["Registration"] = ["MultiResolutionRegistration"]
par_map["NumberOfResolutions"] = ["3"]
par_map["MaximumNumberOfIterations"] = ["500"]

# INITIALIZATION: This snaps the centers of the volumes together
par_map["AutomaticTransformInitialization"] = ["true"]
par_map["AutomaticTransformInitializationMethod"] = ["GeometricalCenter"]
par_obj.AddParameterMap(par_map)

# 2. Create the Registration Object
registration_method = itk.ElastixRegistrationMethod.New(fixed_image, moving_image)

registration_method.SetParameterObject(par_obj)

# 4. Run Registration
registration_method.Update()

# 5. Get Results
result_image = registration_method.GetOutput()
result_transform_parameters = registration_method.GetTransformParameterObject()

# 6. Save the registered HU phantom for visual check
itk.imwrite(result_image, "output/registered_hu_phantom.nrrd")
# Save the transformation to apply to the Label map later
# Save the transformation to a file
result_transform_parameters.WriteParameterFile(result_transform_parameters.GetParameterMap(0), "output/rigid_transform.txt")

print("Rigid registration complete.")