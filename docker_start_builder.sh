#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
CONTAINER="${EXACT_DRAUGHTS_DOCKER_CONTAINER:-exact-draughts-armhf-builder}"
IMAGE="${EXACT_DRAUGHTS_DOCKER_IMAGE:-exact-draughts-armhf-build:bullseye}"

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
        -v "$ROOT:/src/exact-draughts" \
        -w /src/exact-draughts \
        "$IMAGE" \
        sleep infinity >/dev/null
fi

echo "$CONTAINER"
