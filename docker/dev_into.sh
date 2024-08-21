#!/usr/bin/env bash

SATURNV_ROOT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd -P )"
source "$SATURNV_ROOT_DIR/docker/common.sh"

WORKDIR="$RUN_TIME_ROOT"

MAIN_CONTAINER_NAME="tadsim_desktop_$USER"
info "use dev name [\033[32m$MAIN_CONTAINER_NAME\033[0m]"


docker exec \
  -u "$USER" \
  -w "$WORKDIR" \
  -it "$MAIN_CONTAINER_NAME" \
  /bin/bash