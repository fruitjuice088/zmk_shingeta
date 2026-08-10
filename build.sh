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

build_board() {
  local board="$1"
  local builddir="app/build_${board}"
  local pristine="${2:-}"

  run_container '
    if [ ! -f .west/config ]; then
      echo "==> Initial setup: Running west init and update..."
      printf "[manifest]\npath = app\nfile = west.yml\n" > .west/config
      west update
      west zephyr-export
    fi
    west build -s app -d '"$builddir"' '"$pristine"' -b '"$board"'
  '

  mkdir -p .build
  cp "${builddir}/zephyr/zmk.uf2" ".build/${board}.uf2"
  echo "Artifact: .build/${board}.uf2"
}

PRISTINE=""
for arg in "$@"; do
  if [[ "$arg" == "-p" ]]; then
    PRISTINE="-p"
  fi
done

case "${1:-nakid30}" in
  nakid30|build)
    ensure_image
    build_board nakid30_left "$PRISTINE"
    build_board nakid30_right "$PRISTINE"
    ;;

  split)
    ensure_image
    build_shield k30_split_left "$PRISTINE"
    build_shield k30_split_right "$PRISTINE"
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
    rm -rf app/build app/build_nakid30_left app/build_nakid30_right .build
    echo "Removed build cache."
    ;;

  nuke)
    for v in "${VOLUMES[@]}"; do
      docker volume rm "$v" 2>/dev/null && echo "Deleted volume $v." || true
    done
    rm -rf app/build app/build_nakid30_left app/build_nakid30_right .build
    echo "Removed all volumes and build cache."
    ;;

  update)
    ensure_image
    run_container 'west update && west zephyr-export'
    echo "Dependencies are up-to-date."
    ;;

  *)
    echo "Usage: ./build.sh [build|nakid30|split|reset|settings_reset|revxlp|clean|nuke|update] [-p]"
    echo "  (default: nakid30 = nakid30_left + nakid30_right)"
    ;;
esac
