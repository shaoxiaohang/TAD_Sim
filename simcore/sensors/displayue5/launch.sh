#! /bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

function launch_editor(){
  export LD_LIBRARY_PATH="$OPENVR_LIBRARY_PATH:$DISPLAY_LIBRARY_PATH:$LD_LIBRARY_PATH"
  $UE5_ROOT/Engine/Binaries/Linux/UnrealEditor "${DISPLAY_UNREAL_ROOT}/Display.uproject" -vulkan -nosound
}

function launch_game(){
  export LD_LIBRARY_PATH="$DISPLAY_LIBRARY_PATH:$LD_LIBRARY_PATH"
  bash ${DISPLAY_UNREAL_ROOT}/Saved/Linux/Display.sh -game -windowed -NoLoadingScreen WinX=400 WinY=100 ResX=800 ResY=600 -novsync -nosound
}

while [[ $# -gt 0 ]]; do
  case $1 in
    editor)
      launch_editor
      shift
      ;;
    game)
      launch_game
      shift
      ;;
    *)
      echo "Unknown option $1"
      exit 1
      ;;
  esac
done