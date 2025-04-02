#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

cmd=$1
params=""
if [ $# -gt 1 ];then
    params="${@:2}"
fi

function launch_map_test() {
  echo $LD_LIBRARY_PATH
  pushd $TADSIM_ROOT/common/map_sdk/test/build > /dev/null
  info "start running hadmap test $params"
  ./hadmap_test "$params" 
  popd > /dev/null
}

function launch_mapcache_test() {
  pushd $TADSIM_ROOT/simcore/traffic/build/bin > /dev/null
  info "start running mapcache test $params"
  ./txTrafficFramework_test "$params" 
  popd > /dev/null
}

function launch_excel2asam() {
  pushd $TADSIM_ROOT/simcore/excel2asam/build/bin > /dev/null
  info "start running excel2asam test $params"
  ./excel2asam $params
  popd > /dev/null
}

export LD_LIBRARY_PATH=$TADSIM_ROOT/common/map_sdk/hadmap/lib


while [[ $# -gt 0 ]]; do
  case $cmd in
    -h|--help)
      show_usage
      exit 0
      ;;
    map_test)
      launch_map_test
      exit 0
      ;;
    mapcache)
      launch_mapcache_test
      exit 0
      ;;
    excel2asam)
      launch_excel2asam
      exit 0
      ;;
    *)
      error "Unknown option $1"
      exit 1
      ;;
  esac
done