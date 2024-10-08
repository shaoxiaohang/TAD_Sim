#!/usr/bin/env bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
TADSIM_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd -P )"

TADSIM_BUILD_DIR="$TADSIM_ROOT/build"
TADSIM_SERVICE_DIR="$TADSIM_BUILD_DIR/service"
TADSIM_DEPENDENCIES_DIR="$TADSIM_SERVICE_DIR/simdeps"
TADSIM_LOG_DIR="$TADSIM_ROOT/log"
TADSIM_CONFIG_DIR="$HOME/.config/tadsim"
TADSIM_BUILD_LOG_DIR="$TADSIM_LOG_DIR/build"
TADSIM_TEST_LOG_DIR="$TADSIM_LOG_DIR/test"
TADSIM_DEV_LOG_DIR="$TADSIM_LOG_DIR/dev"
TADSIM_DEV_SERVICE_DIR=$TADSIM_BUILD_DIR/release/linux-unpacked/resources/app/service
TADSIM_NODE_ADDON_DIR=$TADSIM_ROOT/simcore/framework/src/node_addon


export LD_LIBRARY_PATH="$TADSIM_DEPENDENCIES_DIR:$LD_LIBRARY_PATH"

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

function create_symlink_if_not_exists() {
    local target_file="$1"
    local link_name="$2"

    if [ ! -e "$link_name" ]; then
        ln -s "$target_file" "$link_name"
        ok "Link created: $link_name -> $target_file"
    fi
}

mkdir -p "$TADSIM_BUILD_LOG_DIR"
mkdir -p "$TADSIM_TEST_LOG_DIR"
mkdir -p "$TADSIM_DEV_LOG_DIR"

create_symlink_if_not_exists $TADSIM_CONFIG_DIR/cache/debug_log/txsim-local-service.ERROR $TADSIM_DEV_LOG_DIR/txsim-local-service.ERROR
create_symlink_if_not_exists $TADSIM_CONFIG_DIR/cache/debug_log/txsim-local-service.INFO $TADSIM_DEV_LOG_DIR/txsim-local-service.INFO
create_symlink_if_not_exists $TADSIM_CONFIG_DIR/cache/debug_log/txsim-local-service.WARNING $TADSIM_DEV_LOG_DIR/txsim-local-service.WARNING