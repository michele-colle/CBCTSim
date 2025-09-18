#!/bin/bash

# Make sure we are in the correct directory (the one containing the slurm scripts)
# cd /path/to/your/scripts

echo "Submitting the Single-Threaded Job Array..."

# Submit the first job and capture its Job ID using the --parsable flag
# The --parsable flag makes sbatch output ONLY the numeric job ID
jobid=$(sbatch --parsable job_array.slurm)

# Check if the job submission was successful
if [ -z "$jobid" ]; then
    echo "Error: Failed to submit job_array.slurm"
    exit 1
fi

echo "Job Array submitted successfully with Job ID: $jobid"

# Submit the second job with a dependency on the first job
# --dependency=afterok:$jobid means "Do not start this job until job $jobid
# has completed successfully. If it fails, cancel this job."
echo "Submitting the Multi-Threaded Job with a dependency on Job ID $jobid..."
sbatch --dependency=afterok:$jobid job_mt.slurm

echo "------------------------------------------------------------------"
echo "All jobs submitted. The MT job will start only after the ST array is complete."
echo "Use 'squeue -u $USER' to monitor the status."
echo "------------------------------------------------------------------"