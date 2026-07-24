#!/bin/bash

ACCOUNT_HOME="$(getent passwd "$(id -un)" | cut -d: -f6)"
VENV_DIR="$ACCOUNT_HOME/.rts3917_build"

if [ ! -f "$VENV_DIR/bin/activate" ]; then
    echo "Creating virtual environment at $VENV_DIR..."
    python3 -m venv "$VENV_DIR" || return 1
fi

source "$VENV_DIR/bin/activate"

echo "Virtual environment: $VIRTUAL_ENV"
echo "Python executable: $(command -v python3)"

python3 -m pip install pycryptodome requests cryptography


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
export HSM_TOKEN='dc2bb143a28832603da0f176aba237f94698d656f3c43525cff186b2de947166'

echo "IPC Camera development environment configured"
echo "Project root: $PROJECT_DIR"
echo "Tools directory: $TOOLS_DIR"
echo "Updated PATH: $PATH"