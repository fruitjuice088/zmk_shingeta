#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="zmk-build-local"
VOLUMES=(zmk-zephyr zmk-modules zmk-tools zmk-west)
BOARD="seeeduino_xiao_ble"

run_container() {
  docker run --rm \
    -v "$(pwd)/app:/workspace/app" \
    -v zmk-west:/workspace/.west \
    -v zmk-zephyr:/workspace/zephyr \
    -v zmk-modules:/workspace/modules \
    -v zmk-tools:/workspace/tools \
    -w /workspace \
    "$IMAGE_NAME" \
    bash -c "$1"
}

ensure_image() {
  echo "==> Building Docker image..."
  docker build -t "$IMAGE_NAME" -f Dockerfile.build .
}

build_shield() {
  local shield="$1"
  local pristine="${2:-}"

  run_container '
    if [ ! -f .west/config ]; then
      echo "==> Initial setup: Running west init and update..."
      printf "[manifest]\npath = app\nfile = west.yml\n" > .west/config
      west update
      west zephyr-export
    fi
    west build -s app -d app/build '"$pristine"' -b '"$BOARD"' -- -DSHIELD='"$shield"'
  '

  mkdir -p .build
  cp app/build/zephyr/zmk.uf2 ".build/${shield}-${BOARD}.uf2"
  echo "Artifact: .build/${shield}-${BOARD}.uf2"
}

PRISTINE=""
for arg in "$@"; do
  if [[ "$arg" == "-p" ]]; then
    PRISTINE="-p"
  fi
done

case "${1:-split}" in
  split|build)
    ensure_image
    build_shield k30_ble_split_left "$PRISTINE"
    build_shield k30_ble_split_right "$PRISTINE"
    ;;

  left)
    ensure_image
    build_shield k30_ble_split_left "$PRISTINE"
    ;;

  right)
    ensure_image
    build_shield k30_ble_split_right "$PRISTINE"
    ;;

  reset|settings_reset)
    ensure_image
    build_shield settings_reset "$PRISTINE"
    ;;

  revxlp)
    ensure_image
    build_shield revxlp30 "$PRISTINE"
    ;;

  clean)
    rm -rf app/build .build
    echo "Removed build cache."
    ;;

  nuke)
    for v in "${VOLUMES[@]}"; do
      docker volume rm "$v" 2>/dev/null && echo "Deleted volume $v." || true
    done
    rm -rf app/build .build
    echo "Removed all volumes and build cache."
    ;;

  update)
    ensure_image
    run_container 'west update && west zephyr-export'
    echo "Dependencies are up-to-date."
    ;;

  *)
    echo "Usage: ./build.sh [split|build|left|right|reset|settings_reset|revxlp|clean|nuke|update] [-p]"
    ;;
esac
