#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

# Setting build parmameters
DISPLAY_ROOT="$(cd "$(dirname "$0")";pwd)"
DISPLAY_BUILD="$DISPLAY_ROOT/build"

export http_proxy=http://10.9.1.251:8838
export https_proxy=http://10.9.1.251:8838

#prerequisites
#echo =================LINUX PREPARE=================
# sh ./download_deps.sh
# mkdir -p $DISPLAY_ROOT/Plugins/BoostLib/deps
# ln -s /usr/include/boost $DISPLAY_ROOT/Plugins/BoostLib/deps/boost

if [ ! -f "${DISPLAY_ROOT}/Makefile" ]; then
  echo "generating make file"
  "$UE5_ROOT/GenerateProjectFiles.sh" -project="${DISPLAY_ROOT}/Display.uproject" -game -engine -makefiles
fi

function build_editor(){
  info "Building DisplayEditor..."
  make DisplayEditor
}

function build_game(){
  info "Building DisplayGame..."
  make Display
  cp ${DISPLAY_UNREAL_ROOT}/Binaries/Linux/Display ${DISPLAY_UNREAL_ROOT}/Saved/Linux/Display/Binaries/Linux
  #cp ${DISPLAY_UNREAL_ROOT}/Binaries/Linux/Display $TADSIM_ROOT/build/release/linux-unpacked/resources/app/service/Display/Display/Binaries/Linux
  #cp ${DISPLAY_UNREAL_ROOT}/Binaries/Linux/Display.sym $TADSIM_ROOT/build/release/linux-unpacked/resources/app/service/Display/Display/Binaries/Linux
  info "Done"
}

function cook(){
  export LD_LIBRARY_PATH="$OPENVR_LIBRARY_PATH:$DISPLAY_LIBRARY_PATH:$LD_LIBRARY_PATH"
  info "Cooking Display..."
  $UE5_ROOT/Engine/Binaries/Linux/UnrealEditor ${DISPLAY_ROOT}/Display.uproject -run=cook -targetplatform=Linux -iterate -map=/Game/Basic
}

function package(){
  export LD_LIBRARY_PATH="$OPENVR_LIBRARY_PATH:$DISPLAY_LIBRARY_PATH:$LD_LIBRARY_PATH"
  info "Packaging Display..."
  cd "$UE5_ROOT/Engine/Build/BatchFiles/"
  ./RunUAT.sh BuildCookRun -utf8output \
                         -platform=Linux \
                         -clientconfig=Development \
                         -serverconfig=Development \
                         -project="$DISPLAY_ROOT/Display.uproject" \
                         -noP4 \
                         -nodebuginfo \
                         -cook \
                         -build \
                         -stage \
                         -prereqs \
                         -pak \
                         -archive \
                         -archivedirectory="$DISPLAY_ROOT/Saved"

#  info "Deploying Display..."

#   # deploy
#   cd "$DISPLAY_ROOT"
#   cp "$DISPLAY_ROOT/Binaries/Linux/Display.sym" "$DISPLAY_ROOT/Saved/StagedBuilds/LinuxNoEditor/Display/Binaries/Linux/"

#   cd "$DISPLAY_ROOT/Saved/StagedBuilds"
#   mv LinuxNoEditor Display
#   cp -r ../../NeuralStyle ./Display/Display/
#   cp -r ../../XMLFiles ./Display/Display/
#   cp -f ../../Display.sh ./Display/Display.sh
#   cp -f ../../Display-cloud.sh ./Display/Display-cloud.sh
#   chmod +x ./Display/Display.sh
#   chmod +x ./Display/Display-cloud.sh
#   cd ../..

#   # clean & mkdir
#   rm -rf "$DISPLAY_ROOT/Build"
#   rm -rf "$DISPLAY_BUILD"
#   mkdir "$DISPLAY_BUILD"
#   mkdir "$DISPLAY_BUILD/bin"

#   # package
#   info "Packaging Display..."
#   mv ./Saved/StagedBuilds/Display $DISPLAY_BUILD/bin/Display
#   rm $DISPLAY_BUILD/bin/Display/Display/Binaries/Linux/ubuntu18_20/libcuda.so.1

#   # zip
#   # cd "$DISPLAY_BUILD/bin"
#   # tar -czf display.tar.gz ./Display

#   cp -r $DISPLAY_BUILD/bin/Display $TADSIM_ROOT/build/release/linux-unpacked/resources/app/service

#   # Change the working directory back to the original directory where the script was run
#   cd "$DISPLAY_ROOT"

  info "Packaging Done"
}
while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
        show_usage
        shift
      ;;
    editor)
      build_editor
      shift
      ;;
    game)
      build_game
      shift
      ;;
    cook)
      cook
      shift
      ;;
    package)
      package
      shift
      ;;
    *)
      echo "Unknown option $1"
      exit 1
      ;;
  esac
done