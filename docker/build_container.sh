#!/bin/bash
ROOT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")"/..)
source "$ROOT_DIR/docker/common.sh"

REGISTRY="hub.hobot.cc"
DESKTOP_TAG=$(get_desktop_tag)
DISPLAY_TAG=$(get_display_tag)

DESKTOP_IMAGE_TAG="$REGISTRY/$DESKTOP_TAG"
DISPLAY_IMAGE_TAG="$REGISTRY/$DISPLAY_TAG"

#UBUNTU_IMAGE=ubuntu:18.04
UBUNTU_IMAGE=nvidia/cuda:11.8.0-cudnn8-devel-ubuntu20.04

function build_desktop_image() {

docker build --build-arg BASE_IMAGE="$UBUNTU_IMAGE" \
                                 -t "$DESKTOP_IMAGE_TAG" \
                                 -f $DOCKER_DIR/Dockerfile \
                                 "$SATURNV_ROOT_DIR/docker"
}

function build_display_image() {
docker build -t "$DISPLAY_IMAGE_TAG" \
            -f $DOCKER_DIR/Dockerfile_display \
            "$SATURNV_ROOT_DIR/docker"
}

function exit_usage() {
  echo -e "\
Usage: build_main_container.sh <action>
  Action:
      --build-base                - Build base docker image.
      --build-main                - Build main docker image.
      --push-base                 - Push base image to the hub registry.
      --push-main                 - Push main image to the hub registry.
      -h, --help                  - Print usage."
  exit "${1:-0}"
}



# Defaults
BUILD_DESKTOP_IMAGE=false
BUILD_DISPLAY_IMAGE=false
PUSH_DESKTOP_IMAGE=false
PUSH_DISPLAY_IMAGE=false

while [ "$#" -gt 0 ]
do
  case "$1" in
    --build-desktop)
      BUILD_DESKTOP_IMAGE=true
      shift
      ;;
    --build-display)
      BUILD_DISPLAY_IMAGE=true
      shift
      ;;
    --push-desktop)
      PUSH_DESKTOP_IMAGE=true
      shift
      ;;
    --push-display)
      PUSH_DISPLAY_IMAGE=true
      shift
      ;;
    --)
      shift
      break
      ;;
    *)
      exit_usage 1
      ;;
  esac
done

if [ "$BUILD_DESKTOP_IMAGE" == "true" ]; then
  build_desktop_image
fi

if [ "$BUILD_DISPLAY_IMAGE" == "true" ]; then
  build_display_image
fi

if [ "$PUSH_DESKTOP_IMAGE" == "true" ]; then
  push_desktop_image
fi

if [ "$PUSH_DISPLAY_IMAGE" == "true" ]; then
  push_display_image
fi