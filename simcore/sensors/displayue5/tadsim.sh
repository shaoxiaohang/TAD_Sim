#! /bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

export LD_LIBRARY_PATH="$DISPLAY_LIBRARY_PATH:$LD_LIBRARY_PATH"
bash ${DISPLAY_UNREAL_ROOT}/Saved/Linux/Display.sh -game -windowed -ResX=600 -ResY=400 -NoLoadingScreen -mode=FrameSync -novsync -nosound -log -abslog=$TADSIM_ROOT/log/dev/ue5.log
