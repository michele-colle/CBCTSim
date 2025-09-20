import os
import numpy as np

# --- Configuration ---
NUM_ST_JOBS = 88  # Creates run_0.mac through run_87.mac
MAX_ANGLE_DEG = 0
NUM_PARTICLES_ST = 100000
NUM_PARTICLES_MT = 8800000  # A much larger run for the MT test
OUTPUT_DIR = "build/cpu-release"

# Base content for the macro files
# We will use format strings to substitute the changing values
MACRO_TEMPLATE = """
/control/verbose 0
/run/verbose 0

/control/execute geo.mac

/run/initialize

/analysis/setFileName {analysis_filename}
/mysim/setObjectAngle {angle_deg} deg
/run/beamOn {num_particles}
"""

def generate_files():
    """Generates all the macro files needed for the Slurm job."""
    
    # Create the output directory if it doesn't exist
    if not os.path.exists(OUTPUT_DIR):
        print(f"Creating directory: {OUTPUT_DIR}")
        os.makedirs(OUTPUT_DIR)
    else:
        print(f"Directory already exists: {OUTPUT_DIR}")

    # --- Generate Single-Threaded (ST) Job Array Macros ---
    print(f"Generating {NUM_ST_JOBS} single-threaded macro files...")
    
    # Create a linearly spaced set of angles from 0 to MAX_ANGLE_DEG
    angles = np.linspace(0, MAX_ANGLE_DEG, NUM_ST_JOBS)
    
    for i in range(NUM_ST_JOBS):
        angle = angles[i]
        macro_filename = f"run_{i}.mac"
        analysis_filename = f"run_{i}"
        
        file_content = MACRO_TEMPLATE.format(
            analysis_filename=analysis_filename,
            angle_deg=angle,
            num_particles=NUM_PARTICLES_ST
        )
        
        filepath = os.path.join(OUTPUT_DIR, macro_filename)
        with open(filepath, 'w') as f:
            f.write(file_content)
            
    print(f"Successfully created run_0.mac to run_{NUM_ST_JOBS - 1}.mac")

    # --- Generate the Multi-Threaded (MT) Macro ---
    print("\nGenerating the multi-threaded macro file...")
    
    mt_macro_filename = "runMT.mac"
    mt_analysis_filename = "runMT"
    
    # For the MT test, we can just use the first angle (0 degrees)
    mt_angle = 0.0
    
    mt_file_content = MACRO_TEMPLATE.format(
        analysis_filename=mt_analysis_filename,
        angle_deg=mt_angle,
        num_particles=NUM_PARTICLES_MT
    )
    
    filepath = os.path.join(OUTPUT_DIR, mt_macro_filename)
    with open(filepath, 'w') as f:
        f.write(mt_file_content)

    print("\nGenerating the test macro file...")
    
    mt_macro_filename = "runTest.mac"
    mt_analysis_filename = "runTest"
    
    # For the MT test, we can just use the first angle (0 degrees)
    mt_angle = 0.0
    
    mt_file_content = MACRO_TEMPLATE.format(
        analysis_filename=mt_analysis_filename,
        angle_deg=mt_angle,
        num_particles=1000
    )
    
    filepath = os.path.join(OUTPUT_DIR, mt_macro_filename)
    with open(filepath, 'w') as f:
        f.write(mt_file_content)
        
    print(f"Successfully created {mt_macro_filename}")


if __name__ == "__main__":
    generate_files()
    print("\nAll files generated successfully.")
