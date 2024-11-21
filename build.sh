#!/bin/bash

# Clean and build the project
echo "Cleaning old builds..."
make clean

echo "Building the project..."
make all

# Check if builds succeeded
if [ -f "nexus" ] && [ -f "registry_server" ]; then
    echo "Build successful!"
    echo "Executables: nexus, registry_server"
else
    echo "Build failed. Check errors."
fi
