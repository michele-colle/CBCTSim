#!/bin/bash
#SBATCH --job-name=g4_mt_c2d        # A descriptive name for the MT job
#SBATCH --nodes=1                   # We want all cores on a single machine
#SBATCH --ntasks=1                  # The entire job is one single process
#SBATCH --cpus-per-task=28          # CRITICAL: This task gets all 88 CPU cores
#SBATCH --partition=c2d      # CRITICAL: This MUST match the name from your blueprint

# A single log file for this specific job
# %j is the unique Job ID for this job
#SBATCH --output=log_mt_%j.txt

# --- Your Application ---

# Activate the Spack environment
echo "Activating Spack environment..."
source ~/spack/share/spack/setup-env.sh
spack env activate g4_cbct_env

# Navigate to the directory where the executable is located
echo "Changing to the build directory..."
cd ~/CBCTSim/build/cpu-release/


# Print some useful information to the log file
echo "========================================================"
echo "Starting MT Job ID: $SLURM_JOB_ID"
echo "Running on host: $(hostname)"
echo "Allocated CPUs: $SLURM_CPUS_PER_TASK"
echo "========================================================"

# Launch the multi-threaded application, telling it to use 28 threads
srun ./particleGun runC2D.mac 28

echo "--- MT Job ID: $SLURM_JOB_ID finished successfully. ---"
