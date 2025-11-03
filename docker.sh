#!/bin/bash

CONTAINER_NAME="india-ubuntu"
IMAGE_NAME="ubuntu:24.04"

# Check if container exists
if ! sudo docker ps -a --filter "name=${CONTAINER_NAME}" | grep -q "${CONTAINER_NAME}"; then
    # Container doesn't exist, create and enter
    echo "Creating new container..."
    sudo docker run -it --name "${CONTAINER_NAME}" \
        -v $(pwd):/home/ubuntu/ipc \
        --privileged \
        "${IMAGE_NAME}" /bin/bash
elif sudo docker ps --filter "name=${CONTAINER_NAME}" | grep -q "${CONTAINER_NAME}"; then
    # Container is running, enter directly
    echo "Entering running container..."
    sudo docker exec -it "${CONTAINER_NAME}" /bin/bash
else
    # Container exists but stopped, start and enter
    echo "Starting stopped container..."
    sudo docker start "${CONTAINER_NAME}" >/dev/null
    sudo docker exec -it "${CONTAINER_NAME}" /bin/bash
fi
