#!/usr/bin/env bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

TADSIM_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../.." && pwd -P )"
DOCKER_HOME="/home/$USER"
DISPLAY_UNREAL_ROOT=$SCRIPT_DIR
export TADSIM="/saturnv/build"

OPENVR_LIBRARY_PATH=$UE5_ROOT/Engine/Source/ThirdParty/OpenVR/OpenVRv1_5_17/lib/linux64
DISPLAY_LIBRARY_PATH=$DISPLAY_UNREAL_ROOT/Binaries/Linux/ubuntu18_20

BOLD='\033[1m'
RED='\033[0;31m'
GREEN='\033[32m'
WHITE='\033[34m'
YELLOW='\033[33m'
NO_COLOR='\033[0m'

function info() {
  (>&2 echo -e "[${WHITE}${BOLD}INFO${NO_COLOR}] $*")
}

function error() {
  (>&2 echo -e "[${RED}ERROR${NO_COLOR}] $*")
}

function warning() {
  (>&2 echo -e "${YELLOW}[WARNING] $*${NO_COLOR}")
}

function ok() {
  (>&2 echo -e "[${GREEN}${BOLD} OK ${NO_COLOR}] $*")
}
