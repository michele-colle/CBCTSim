#!/bin/bash
#SBATCH --job-name=g4_st_array      # A descriptive name for the job array
#SBATCH --nodes=1                   # Each task will run on a single node
#SBATCH --ntasks=1                  # Each task is a single process
#SBATCH --cpus-per-task=1           # CRITICAL: Each task gets exactly one CPU core
#SBATCH --array=0-87                # CRITICAL: This creates 89 tasks, numbered 0 through 8/
#SBATCH --partition=h3_partition      # CRITICAL: This MUST match the name from your blueprint

# Slurm will automatically create separate log files for each task in the array
# %A is the Job ID of the whole array, %a is the specific task number (0-87)
#SBATCH --output=log_st_%A_%a.txt

# --- Your Application ---

# Define the path to your executable
APP_DIR="build/cpu-release"

# Print some useful information to the log file for each task
echo "========================================================"
echo "Starting ST Task ID: $SLURM_ARRAY_TASK_ID"
echo "Running on host: $(hostname)"
echo "Allocated CPUs: $SLURM_CPUS_PER_TASK"
echo "========================================================"

# Use the special Slurm variable $SLURM_ARRAY_TASK_ID to select the correct macro file
# srun is the best practice for launching tasks inside a Slurm allocation
srun "$APP_DIR/particleGun" "$APP_DIR/run_${SLURM_ARRAY_TASK_ID}.mac" 1

echo "--- ST Task ID: $SLURM_ARRAY_TASK_ID finished successfully. ---"
