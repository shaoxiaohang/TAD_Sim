#! /bin/bash


sudo apt-get update \
    && apt-get upgrade -y \
    && apt-get install -y --no-install-recommends \
    libegl1 \
    libxext6 \
    vulkan-tools

sudo mkdir -p /opt/vulkan

sudo tar -xvf /saturnv/vulkansdk-linux-x86_64-1.3.280.0.tar.xz -C /opt/vulkan

