#!/bin/bash

export http_proxy=http://10.9.1.251:8838
export https_proxy=http://10.9.1.251:8838

# Setting build parmameters
RADAR_ROOT="$(cd "$(dirname "$0")";pwd)"
RADAR_BUILD="$RADAR_ROOT/build"

# clean & mkdir
rm -rf "$RADAR_BUILD"
mkdir "$RADAR_BUILD"

# build
cd "$RADAR_BUILD"
wget https://raw.githubusercontent.com/metayeti/mINI/master/src/mini/ini.h -O ./ini.h
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja -j8

# Change the working directory back to the original directory where the script was run
cd "$RADAR_ROOT"
