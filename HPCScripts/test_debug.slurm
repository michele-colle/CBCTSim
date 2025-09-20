#!/bin/bash

# --- SBATCH Directives for the Debug Partition ---

#SBATCH --job-name=debug-test         # A clear name for the job
#SBATCH --partition=debug             # CRITICAL: Target the healthy 'debug' partition

# Request minimal resources
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --time=00:10:00               # 10-minute time limit, which is plenty

# Send the console output to a dedicated log file
# The %j will be replaced by the unique Job ID
#SBATCH --output=/apps/CBCTSim/logs/debug_test_%j.log


# --- Execution Block (Commands that run on the compute node) ---

echo "========================================================"
echo "DEBUG JOB STARTED"
echo "Job ID: $SLURM_JOB_ID"
echo "Running on host: $(hostname)"
echo "========================================================"

# 1. Activate the Spack environment to find Geant4 and other libraries
echo "Activating Spack environment..."
source /apps/spack/share/spack/setup-env.sh
spack env activate g4_cbct_env

# 2. Navigate to the directory where the executable is located
# This is crucial because the macro assumes the executable is run from here.
echo "Changing to the build directory..."
cd /apps/CBCTSim/build/cpu-release/

# 3. Run the lightweight simulation
echo "Executing particleGun with runTest.mac..."
./particleGun runTest.mac

echo "========================================================"
echo "DEBUG JOB FINISHED"
echo "Checking for the output ROOT file..."

# Verify that the ROOT file was created. This will be saved in the log.
ls -l runTest.root

echo "========================================================"
```    Save and exit the editor (`Ctrl+X`, then `Y`, then `Enter`).
