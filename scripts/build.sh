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

function build_sim_traffic() {
  pushd $TADSIM_ROOT/simcore > /dev/null
  build_project traffic build.sh
  cp ./traffic/build/bin/txSimTraffic $TADSIM_DEV_SERVICE_DIR/traffic
  popd > /dev/null
}

function build_sim_planning() {
  pushd $TADSIM_ROOT/simcore > /dev/null
  build_project perfect_planning build.sh
  cp ./perfect_planning/build/bin/libtx_perfect_planning.so $TADSIM_DEV_SERVICE_DIR/perfect_planning
  popd > /dev/null
}

function build_map_sdk {
  pushd $TADSIM_ROOT/common > /dev/null
  build_project map_sdk build.sh
  popd > /dev/null
}

function build_sim_label() {
  pushd $TADSIM_ROOT/simcore/sensors > /dev/null
  build_project sim_label build.sh
  cp ./sim_label/build/lib/libsim_label.so  $TADSIM_DEV_SERVICE_DIR/sim_label
  popd > /dev/null
}

function build_map_test() {
  pushd $TADSIM_ROOT/common/map_sdk/test > /dev/null
  rm -rf build
  mkdir -p build
  cd build
  cmake ..
  make -j
  popd > /dev/null
}

function build_message() {
  pushd $TADSIM_ROOT/common > /dev/null
  build_project "message" "generate_cpp.sh"
  #cp $TADSIM_ROOT/common/message/build/*  $TADSIM_DISPLAY_DIR/Source/Display/SimMsg
  popd > /dev/null
}

function build_map_server() {
  pushd $TADSIM_ROOT/simcore > /dev/null
  build_project map_server build.sh
  cp ./map_server/build/bin/libmap_parser.a  $TADSIM_DEV_SERVICE_DIR
  cp ./map_server/build/bin/libOpenDrivePlugin.so $TADSIM_DEV_SIM_DEP_DIR
  cp ./map_server/build/bin/libscene_wrapper.so $TADSIM_DEV_SIM_DEP_DIR
  cp ./map_server/build/bin/txSimService $TADSIM_DEV_SERVICE_DIR
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
    message)
      build_message
      shift
      ;;
    map_server)
      build_map_server
      shift
      ;;
    sim_label)
      build_sim_label
      shift
      ;;
    planning)
      build_sim_planning
      shift
      ;;
    traffic)
      build_sim_traffic
      shift
      ;;
    map_test)
      build_map_test
      shift
      ;;
    map_sdk)
      build_map_sdk
      shift
      ;;
    *)
      error "Unknown option $1"
      exit 1
      ;;
  esac
done