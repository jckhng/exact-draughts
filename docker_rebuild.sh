#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
CONTAINER="$("$ROOT/docker_start_builder.sh" | tail -n 1)"
UID_HOST="$(id -u)"
GID_HOST="$(id -g)"
MAKE_TARGETS="${KINDLE_DRAUGHTS_MAKE_TARGETS:-kindle-draughts smoke-test}"
DO_PACKAGE="${KINDLE_DRAUGHTS_PACKAGE:-1}"

docker exec "$CONTAINER" chown -R "$UID_HOST:$GID_HOST" /src/kindle-draughts
docker exec --user "$UID_HOST:$GID_HOST" "$CONTAINER" /bin/sh -lc "make $MAKE_TARGETS && ./smoke-test"

if [ "$DO_PACKAGE" = "1" ]; then
    KINDLE_DRAUGHTS_DOCKER_CONTAINER="$CONTAINER" "$ROOT/package_extension.sh"
fi

echo "Builder container: $CONTAINER"
echo "Binary: $ROOT/kindle-draughts"
if [ "$DO_PACKAGE" = "1" ]; then
    echo "Package: $ROOT/dist/kindle-draughts-extension.zip"
fi
