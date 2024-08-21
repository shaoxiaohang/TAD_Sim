#!/usr/bin/env bash

SATURNV_ROOT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")"/..)


BOLD='\033[1m'
RED='\033[0;31m'
GREEN='\033[32m'
WHITE='\033[34m'
YELLOW='\033[33m'
NO_COLOR='\033[0m'
CONTAINER_PREFIX=${CONTAINER_PREFIX:-saturnv-sim}
RUN_TIME_ROOT="/saturnv"
REGISTRY="hub.hobot.cc"
DOCKER_DIR=${SATURNV_ROOT_DIR}/docker

function info() {
  (>&2 echo -e "[${WHITE}${BOLD}INFO${NO_COLOR}] $LOG_NAMESPACE$*")
}

function error() {
  (>&2 echo -e "[${RED}ERROR${NO_COLOR}] $LOG_NAMESPACE$*")
}

function warning() {
  (>&2 echo -e "${YELLOW}[WARNING] $LOG_NAMESPACE$*${NO_COLOR}")
}

function ok() {
  (>&2 echo -e "[${GREEN}${BOLD} OK ${NO_COLOR}] $LOG_NAMESPACE$*")
}

function get_desktop_tag() {
  cat "$SATURNV_ROOT_DIR/docker/desktop.tag"
}

function get_display_tag() {
  cat "$SATURNV_ROOT_DIR/docker/display.tag"
}