#!/bin/bash
set -ex

# Install base dependencies
yum install -y git cmake3 python3-pip

# Clone Spack
git clone https://github.com/spack/spack.git ~/spack
chown -R slurm:slurm ~/spack
. ~/spack/share/spack/setup-env.sh

# Clone your application repository (which contains the spack.yaml)
git clone https://github.com/michele-colle/CBCTSim.git ~/CBCTSim
chown -R slurm:slurm ~/CBCTSim

# --- NEW SECTION: Build the Spack Environment from your YAML file ---

# 1. Create a named Spack environment from your repository's yaml file.
#    This gives you a persistent environment you can activate later.
echo "Creating Spack environment from spack.yaml..."
spack env create g4_cbct_env ~/CBCTSim/HPC_env_settings/spack.yaml

# 2. Activate the new environment.
spack env activate g4_cbct_env

# 3. Tell Spack to install everything defined in the environment file.
#    Spack is smart and will read the spack.yaml and build all the specs.
#    This is the step that will take a long time.
echo "Installing all packages defined in the environment..."
spack install -j $(nproc)

# 4. Make the environment easy to use for everyone
#    (Optional but recommended)
echo "source ~/spack/share/spack/setup-env.sh" >> /etc/profile.d/spack.sh

echo "======================================================================"
echo "Setup complete. To use the software, users should run:"
echo "'spack env activate g4_cbct_env'"
echo "======================================================================"
