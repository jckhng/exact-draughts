#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
CONTAINER="${KINDLE_DRAUGHTS_DOCKER_CONTAINER:-kindle-draughts-armhf-builder}"
IMAGE="${KINDLE_DRAUGHTS_DOCKER_IMAGE:-kindle-draughts-armhf-build:bullseye}"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    "$ROOT/docker_build_image.sh" >/dev/null
fi

if docker container inspect "$CONTAINER" >/dev/null 2>&1; then
    if [ "$(docker inspect -f '{{.State.Running}}' "$CONTAINER")" != "true" ]; then
        docker start "$CONTAINER" >/dev/null
    fi
else
    docker run -d \
        --platform linux/arm/v7 \
        --name "$CONTAINER" \
        -v "$ROOT:/src/kindle-draughts" \
        -w /src/kindle-draughts \
        "$IMAGE" \
        sleep infinity >/dev/null
fi

echo "$CONTAINER"
