#!/bin/bash
#SBATCH --job-name=g4_mt_big        # A descriptive name for the MT job
#SBATCH --nodes=1                   # We want all cores on a single machine
#SBATCH --ntasks=1                  # The entire job is one single process
#SBATCH --cpus-per-task=88          # CRITICAL: This task gets all 88 CPU cores

# A single log file for this specific job
# %j is the unique Job ID for this job
#SBATCH --output=log_mt_%j.txt

# --- Your Application ---

# Define the path to your executable
APP_DIR="build/cpu-release"

# Print some useful information to the log file
echo "========================================================"
echo "Starting MT Job ID: $SLURM_JOB_ID"
echo "Running on host: $(hostname)"
echo "Allocated CPUs: $SLURM_CPUS_PER_TASK"
echo "========================================================"

# Launch the multi-threaded application, telling it to use 88 threads
srun "$APP_DIR/particleGun" "$APP_DIR/runMT.mac" 88

echo "--- MT Job ID: $SLURM_JOB_ID finished successfully. ---"