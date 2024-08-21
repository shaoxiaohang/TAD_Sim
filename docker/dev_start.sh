#!/usr/bin/env bash

SATURNV_ROOT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd -P )"
source "$SATURNV_ROOT_DIR/docker/common.sh"


USER_ID=$(id -u)
GRP=$(id -g -n)
GRP_ID=$(id -g)
DOCKER_HOME="/home/$USER"
OUTPUT_HOME=$HOME

# Main docker image
DESKTOP_TAG=$(get_desktop_tag)
MAIN_IMAGE="$REGISTRY/$DESKTOP_TAG"
UE5_ROOT="/home/dpx/workspace/ue5"

function local_volumes() {

  volumes="-v $SATURNV_ROOT_DIR:$RUN_TIME_ROOT \
          -v $OUTPUT_HOME/.cache:$DOCKER_HOME/.cache \
          -v /etc/machine-id:/etc/machine-id \
          -v /etc/passwd-s3fs:/etc/passwd-s3fs \
          -v /etc/passwd-machine:/etc/passwd-machine \
          -v /etc/localtime:/etc/localtime:ro \
          -v /etc/shadow:/etc/shadow \
          -v /var/run/dbus:/var/run/dbus \
          -v $UE5_ROOT:$DOCKER_HOME/ue5 \
          -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
          -v /usr/share/vulkan/icd.d:/usr/share/vulkan/icd.d"
  echo "$volumes"
}


function stop_and_remove_container() {
  docker stop "$1" > /dev/null 2>&1
  docker rm "$1" > /dev/null 2>&1
}



function main(){

    if [ "$USER" == "root" ]; then
      warning "Running dev_start.sh as 'root' is not recommended and can" \
              "cause many problems."
      echo -n "Press ctrl-c to abort, any other key will continue..."
      read -r -n 1
    fi

    MAIN_CONTAINER_NAME="tadsim_desktop_$USER"

    stop_and_remove_container "$MAIN_CONTAINER_NAME"


    local display="$DISPLAY"
    if [ -z "$DISPLAY" ];then
      # The env variable DISPLAY is empty when logging in with SSH, so we must find the correct value
      # on the remote host. otherwise,the xrt_visualizer will fail to open.
      # look up remote host env DISPLAY
      HOST_DISPLAY=$(for file in /proc/[0-9]*; do grep -ao 'DISPLAY=[^[:cntrl:]]*' \
      "$file"/environ 2>/dev/null;done | head -1 | cut -d '=' -f2)
      display="$HOST_DISPLAY"
      [ -z "$HOST_DISPLAY" ] && display=":0"
    fi


    NVIDIA_OPTION=""
    NVIDIA_DRIVER_CAPABILITIES=""

    if command -v nvidia-container-runtime > /dev/null; then
      NVIDIA_OPTION="--runtime=nvidia"
      NVIDIA_DRIVER_CAPABILITIES="all"
    else
      warning "nvidia-container-runtime not found! No Nvidia GPU Support!"
      info "If Nvidia GPU available, please install 'nvidia-container-runtime'"
    fi

    MAX_CPU_FOR_DOCKER=$(( $(nproc) -1 ))
    CPUSETS="0-$MAX_CPU_FOR_DOCKER"

    # Start the main image
    docker run -itd \
        --rm \
        --cpuset-cpus=$CPUSETS \
        $NVIDIA_OPTION \
        --privileged \
        --name "$MAIN_CONTAINER_NAME" \
        -e DISPLAY="$display" \
        -e UE5_ROOT="$UE5_ROOT" \
        -e NVIDIA_DRIVER_CAPABILITIES="$NVIDIA_DRIVER_CAPABILITIES" \
        -e DOCKER_USER="$USER" \
        -e USER="$USER" \
        -e DOCKER_USER_ID="$USER_ID" \
        -e DOCKER_GRP="$GRP" \
        -e DOCKER_GRP_ID="$GRP_ID" \
        -e DOCKER_IMG="$MAIN_IMAGE" \
        --net=host \
        --pid=host \
        $(local_volumes) \
        $MAIN_IMAGE \
        /bin/bash > /dev/null

    if [ $? -ne 0 ]; then
      error "Failed to start docker container \"$MAIN_CONTAINER_NAME\"" \
            "based on image: $MAIN_IMAGE"
      exit 1
    else
      info "$MAIN_CONTAINER_NAME:" \
           "$(docker ps -a | grep "$MAIN_CONTAINER_NAME" | head -c 12)"
    fi


    if [ "$USER" != "root" ]; then
      docker cp /etc/group "$MAIN_CONTAINER_NAME":/etc/group
      chmod 777 "${SATURNV_ROOT_DIR}/docker/docker_add_user.sh"
      docker exec "$MAIN_CONTAINER_NAME" \
          bash -c "$RUN_TIME_ROOT/docker/docker_add_user.sh"
    fi

    # Remove sudo warning
    docker exec -u "$USER" "$MAIN_CONTAINER_NAME" \
        bash -c 'touch ~/.sudo_as_admin_successful'

    ok "Now you can enter into container using: bash devtools/main/dev_into.sh"
}


main