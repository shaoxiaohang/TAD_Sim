#! /bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

export LD_LIBRARY_PATH="$DISPLAY_LIBRARY_PATH:$LD_LIBRARY_PATH"
cp /home/aaa/workspace/hsim/log/Game.ini /home/aaa/workspace/hsim/log/Saved/Config/Linux
bash ${DISPLAY_UNREAL_ROOT}/Saved/Linux/Display.sh -UserDir=$TADSIM_ROOT/log -game -windowed -ResX=800 -ResY=600 -NoLoadingScreen -mode=FrameSync -novsync -nosound -log -abslog=$TADSIM_ROOT/log/dev/ue5.log
