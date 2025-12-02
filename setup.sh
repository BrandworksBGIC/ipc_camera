#!/bin/bash

# Check if virtual environment exists, create if not
if [ ! -d "$HOME/.rts3917_build" ]; then
    echo "Creating virtual environment..."
    sudo apt install make autoconf libtool bzip2 g++ unzip  autopoint automake pkg-config automake pkg-config autoconf m4 bison flex device-tree-compiler python3-venv
    python3 -m venv $HOME/.rts3917_build
    echo "Virtual environment created at $HOME/.rts3917_build"
else
    echo "Virtual environment already exists at $HOME/.rts3917_build"
fi

# Activate virtual environment
source $HOME/.rts3917_build/bin/activate

# Install required packages
pip install pycryptodome
pip install requests
pip install cryptography



# Setup script for IPC Camera project
# This script configures the environment for the IPC camera development

# Get the absolute path of the project directory
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Add tools directory to PATH
TOOLS_DIR="$PROJECT_DIR/tools"
if [ -d "$TOOLS_DIR" ]; then
    # Check if tools directory is already in PATH
    if [[ ":$PATH:" != *":$TOOLS_DIR:"* ]]; then
        export PATH="$TOOLS_DIR:$PATH"
        echo "Added $TOOLS_DIR to PATH"
    else
        echo "Tools directory $TOOLS_DIR is already in PATH"
    fi
else
    echo "Warning: Tools directory $TOOLS_DIR not found"
fi

# Add other useful environment variables
export PROJECT_DIR="$PROJECT_DIR"
export IPC_CAM_ROOT="$PROJECT_DIR"

export JMAKE_CFLAGS="-I$(pwd)/cam/include/"

export HSM_SERVER_IP='127.0.0.1'
export HSM_TOKEN='3bf1a8f69584ca8cf900eb2217813fa3eea1d3a56345ff60431cf78fc6e7b099'

echo "IPC Camera development environment configured"
echo "Project root: $PROJECT_DIR"
echo "Tools directory: $TOOLS_DIR"
echo "Updated PATH: $PATH"