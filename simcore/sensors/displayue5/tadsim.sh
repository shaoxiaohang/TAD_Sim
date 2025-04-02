#! /bin/bash

#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
SATURNV_ROOT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../../" && pwd )"

source "$SATURNV_ROOT_DIR/scripts/base.sh"

export LD_LIBRARY_PATH="$UE_LIBRARY_PATH:$LD_LIBRARY_PATH"

info "Starting ue5..."

echo $SATURNV_ROOT_DIR


cp /home/aaa/workspace/hsim/log/Game.ini /home/aaa/workspace/hsim/log/Saved/Config/Linux
bash ${DISPLAY_UNREAL_ROOT}/Saved/Linux/Display.sh SH1 -UserDir=$TADSIM_ROOT/log -game -windowed -ResX=800 -ResY=600 -NoLoadingScreen -mode=FrameSync -novsync -nosound -log -abslog=$SATURNV_ROOT_DIR/log/dev/ue5.log
