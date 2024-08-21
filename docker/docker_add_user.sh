#!/usr/bin/env bash

if ! getent group | grep "${DOCKER_USER}:x" > /dev/null 2>&1; then
  addgroup --quiet --gid "${DOCKER_GRP_ID}" "${DOCKER_GRP}"
fi
adduser --quiet --disabled-password --force-badname --gecos '' "${DOCKER_USER}" \
        --uid "${DOCKER_USER_ID}" --gid "${DOCKER_GRP_ID}" 2>/dev/null
usermod -a -G input "${DOCKER_USER}"
usermod -a -G sudo "${DOCKER_USER}"

echo "$USER ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Change ownership of the HOME directory
chown "${DOCKER_USER}:${DOCKER_GRP}" "/home/${DOCKER_USER}"
find "/home/${DOCKER_USER}" -maxdepth 1 -name '.??*' ! -path '*/.cache' -exec chown -R "${DOCKER_USER}:${DOCKER_GRP}" {} +

if [ "$SKIP_CHOWN_CACHE" != "yes" ]; then
  chown -R "${DOCKER_USER}:${DOCKER_GRP}" "/home/${DOCKER_USER}/.cache" > /dev/null 2>&1
fi