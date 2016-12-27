#!/bin/bash
if [ -z $1 ] ; then
gnu_version="4.6.5"
else
gnu_version=$1
fi
###########################################################
#
#
# Gnuplot setup
#
#
###########################################################
 
# Starting setup of Gnuplot
echo "Setting up Gnuplot..."
 
 
#----------------------------------------------------------
# Installing dependent packages
#----------------------------------------------------------
 
# Inform the user about the next action
echo "Installing the dependent packages build-essentials g++ gcc..."
 
# Execute the action
sudo apt-get -y install build-essentials g++ gcc
 
 
#----------------------------------------------------------
# Installing Gnuplot
#----------------------------------------------------------
 
# Inform the user about the next action
echo "Downloading and installing Gnuplot..."
 
# Constant values definitions
FOLDER_NAME="Gnuplot"
 
# Create a new folder for storing the source code
mkdir ${FOLDER_NAME}
 
# Change directory
cd ${FOLDER_NAME}
 
# Download source code
#wget http://sourceforge.net/projects/gnuplot/files/gnuplot/4.6.5/gnuplot-4.6.5.tar.gz
wget http://sourceforge.net/projects/gnuplot/files/gnuplot/$gnu_version/gnuplot-${gnu_version}.tar.gz
 
# Extract archive
tar -xvzf gnuplot-${gnu_version}.tar.gz
 
# Change directory
cd gnuplot-${gnu_version}
 
# Configure gnuplot for compilation
./configure
 
# Compile the project
make
 
# Install gnuplot in the default location
sudo make install
 
# Return to the parent directory
cd ../../
 
# Inform user that Gnuplot was successfully installed
echo "Gnuplot was successfully installed."
