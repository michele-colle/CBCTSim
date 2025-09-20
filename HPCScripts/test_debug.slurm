#!/bin/bash

# --- SBATCH Directives ---

#SBATCH --job-name=final-home-test
#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --time=00:10:00

# CRITICAL CHANGE: Set the output log file path to your home directory.
# We use the '~' shortcut. The filename will be unique for each job.
#SBATCH --output=/home/michelecolle108_gmail_com/final_test_log_%j.log


# --- Execution Block (Commands that run on the compute node) ---

echo "========================================================"
echo "FINAL TEST JOB STARTED"
echo "All output will be directed to the home directory."
echo "Job ID: $SLURM_JOB_ID"
echo "Running on host: $(hostname)"
echo "========================================================"

# --- THE CRITICAL FIX: A 'WAIT LOOP' FOR THE SHARED DRIVE ---
echo "Waiting for the /apps directory to be mounted..."
while [ ! -d "/apps/CBCTSim" ]; do
  sleep 2
done
echo "/apps directory is now available."
# -------------------------------------------------------------


# Activate the Spack environment
echo "Activating Spack environment..."
source /apps/spack/share/spack/setup-env.sh
spack env activate g4_cbct_env

# Navigate to the directory where the executable is located
echo "Changing to the build directory..."
cd /apps/CBCTSim/build/cpu-release/

# Run the lightweight simulation.
# This creates runTest.root in the current directory (/apps/CBCTSim/build/cpu-release/).
echo "Executing particleGun with runTest.mac..."
./particleGun runTest.mac

echo "Simulation finished."

# --- Copy the result file to your home directory ---
echo "--------------------------------------------------------"
echo "Copying output ROOT file to home directory..."

# Define source and destination paths
SOURCE_FILE="runTest.root"
# The destination filename will be unique using the Job ID
DESTINATION_FILE="/home/michelecolle108_gmail_com/runTest_job_${SLURM_JOB_ID}.root"

# Copy the file
cp -v "$SOURCE_FILE" "$DESTINATION_FILE"

echo "--------------------------------------------------------"
echo "========================================================"
echo "FINAL TEST JOB FINISHED"
echo "========================================================"
