#!/bin/bash

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

echo "IPC Camera development environment configured"
echo "Project root: $PROJECT_DIR"
echo "Tools directory: $TOOLS_DIR"
echo "Updated PATH: $PATH"