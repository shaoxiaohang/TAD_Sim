#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

function build_project() {
  project="$1"
  build_script="$2"
  info "=== Begin build $project"
  pushd `pwd`/$project > /dev/null
  "./$build_script" > $TADSIM_BUILD_LOG_DIR/$project.log
  popd > /dev/null
  ok "=== End build $project"
}

function build_framwork() {
  pushd $TADSIM_ROOT/simcore > /dev/null
  build_project framework build.sh
  cp ./framework/build/bin/txsim-local-service  $TADSIM_DEV_SERVICE_DIR
  cp ./framework/build/bin/txsim-module-launcher $TADSIM_DEV_SERVICE_DIR
  cp ./framework/src/node_addon/build/Release/txsim-play-service.node $TADSIM_DEV_SERVICE_DIR
  popd > /dev/null
}

while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
      show_usage
      shift
      ;;
    framework)
      build_framwork
      shift
      ;;
    *)
      error "Unknown option $1"
      exit 1
      ;;
  esac
done