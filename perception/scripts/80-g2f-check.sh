#!/bin/bash
# Copyright (c) 2024, GO2FUTURE S.A.
#
# This script must be copied into /opt/nvidia/entrypoint.d inside the container
# and it will be executed by the entrypoint script before starting the application.

set -e

echo -e "\e[32mStarting GO2FUTURE Perception Container\e[0m"

if [ -f "/app/build/perception" ]; then
    echo "/app/build/perception exists."
else
    # Build App
    echo "Building App..."
    pushd "/app"
    rm -rf build && mkdir -p build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/storage ..
    make -j$(nproc)
    popd
fi
