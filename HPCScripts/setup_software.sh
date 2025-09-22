#!/bin/bash
set -ex
# The user who originally ran this script with sudo
TARGET_USER=$SUDO_USER
# Install base dependencies
echo "Installing base dependencies as root..."

yum install -y git cmake3 python3-pip

# Clone Spack
# Switch to the target user to clone and configure Spack
echo "Cloning and configuring Spack as user: $TARGET_USER"
sudo -u $TARGET_USER bash <<EOF
set -ex
# Clone Spack into the user's home directory
git clone https://github.com/spack/spack.git /home/$TARGET_USER/spack
# Source the Spack environment script
. /home/$TARGET_USER/spack/share/spack/setup-env.sh
EOF

echo "Cloning application and setting up Spack environment as user: $TARGET_USER"
sudo -u $TARGET_USER bash <<EOF
set -ex
# Clone your application repository
# the folder should already exists

# Set up the Spack environment from your YAML file
cd /home/$TARGET_USER/CBCTSim
. /home/$TARGET_USER/spack/share/spack/setup-env.sh
spack env create g4_cbct_env HPC_env_settings/spack.yaml
spack env activate g4_cbct_env
spack install -j \$(nproc)
pip3 install numpy
EOF



# 4. Make the environment easy to use for everyone
#    (Optional but recommended)
echo "source /home/$TARGET_USER/spack/share/spack/setup-env.sh" > /etc/profile.d/spack.sh

echo "======================================================================"
echo "Setup complete. To use the software, users should run:"
echo "'spack env activate g4_cbct_env'"
echo "======================================================================"
