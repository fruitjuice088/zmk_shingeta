#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="${IMAGE_NAME:-zmk-build-local}"
VOLUMES=(zmk-zephyr zmk-modules zmk-tools zmk-west)

BOARD="${BOARD:-seeeduino_xiao_ble}"
KEYNUM="${KEYNUM:-30}"
SHIELD="${SHIELD:-revxlp${KEYNUM}}"
KEYMAP="${KEYMAP:-app/snippets/fj88/k${KEYNUM}.keymap}"
KEY_LAYOUT="${KEY_LAYOUT:-2vv3332+2 2+23332vv}"
KEYMAP_NAME="$(basename "${KEYMAP%.keymap}")"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$SCRIPT_DIR"

run_container() {
  local script="$1"
  docker run --rm \
    -e BOARD="$BOARD" \
    -e SHIELD="$SHIELD" \
    -e KEYMAP="$KEYMAP" \
    -e KEY_LAYOUT="$KEY_LAYOUT" \
    -v "${WORKSPACE_ROOT}:/workspace" \
    -v zmk-west:/workspace/.west \
    -v zmk-zephyr:/workspace/zephyr \
    -v zmk-modules:/workspace/modules \
    -v zmk-tools:/workspace/tools \
    -w /workspace \
    "$IMAGE_NAME" \
    bash -euo pipefail -c "$script"
}

build_image() {
  echo "==> Building Docker image..."
  docker build -t "$IMAGE_NAME" -f Dockerfile.build .
}

ensure_west_workspace() {
  run_container '
    if [ ! -f .west/config ]; then
      echo "==> Initial setup: configuring west workspace..."
      printf "[manifest]\npath = app\nfile = west.yml\n" > .west/config
      west update --fetch-opt=--filter=tree:0
      west zephyr-export
    fi
  '
}

build_firmware() {
  local pristine_flag="$1"
  run_container "
    west build -s app -d app/build ${pristine_flag} -b \"${BOARD}\" -- -DSHIELD=\"${SHIELD}\"
  "
}

copy_firmware_artifact() {
  mkdir -p .build

  local source_path
  local extension
  if [[ -f app/build/zephyr/zmk.uf2 ]]; then
    source_path="app/build/zephyr/zmk.uf2"
    extension="uf2"
  elif [[ -f app/build/zephyr/zmk.bin ]]; then
    source_path="app/build/zephyr/zmk.bin"
    extension="bin"
  else
    echo "ERROR: firmware artifact not found under app/build/zephyr" >&2
    exit 1
  fi

  cp "$source_path" ".build/${SHIELD}-${BOARD}.${extension}"
  echo "Artifact: .build/${SHIELD}-${BOARD}.${extension}"
}

generate_keymap_assets() {
  run_container '
    KM=/tmp/keymap-for-drawer.keymap
    sed -e "s/&je /\&kp /g" -e "s/&jmt /\&mt /g" -e "s/&jlt /\&lt /g" \
        -e "s/JP_COLN/COLON/g" -e "s/JP_RT/GT/g" -e "s/JP_//g" \
      "$KEYMAP" > "$KM"

    CFG=app/snippets/fj88/keymap-drawer/config.yaml
    keymap -c "$CFG" parse -z "$KM" -l BASE JPN PAD SYM FUNC MSE OPT \
      | sed "1s|^layout:.*|layout: {cols_thumbs_notation: \"${KEY_LAYOUT}\"}|" \
      > app/build/keymap.yaml

    keymap -c "$CFG" draw app/build/keymap.yaml -s BASE PAD SYM --keys-only -o app/build/keymap.svg
    keymap -c "$CFG" draw app/build/keymap.yaml -s BASE PAD SYM --combos-only -o app/build/keymap-combos.svg
  '

  cp app/build/keymap.yaml ".build/${KEYMAP_NAME}.yaml"
  cp app/build/keymap.svg ".build/${KEYMAP_NAME}.svg"
  cp app/build/keymap-combos.svg ".build/${KEYMAP_NAME}-combos.svg"
  echo "Keymap:   .build/${KEYMAP_NAME}.yaml .build/${KEYMAP_NAME}.svg .build/${KEYMAP_NAME}-combos.svg"
}

ACTION="${1:-build}"
PRISTINE=""
if [[ "$ACTION" == "-p" ]]; then
  ACTION="build"
  PRISTINE="-p"
elif [[ "${2:-}" == "-p" ]]; then
  PRISTINE="-p"
fi

case "$ACTION" in
  build)
    if [[ ! -f "$KEYMAP" ]]; then
      echo "ERROR: keymap file not found: $KEYMAP" >&2
      exit 1
    fi

    build_image
    ensure_west_workspace
    build_firmware "$PRISTINE"
    copy_firmware_artifact
    generate_keymap_assets
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
    build_image
    ensure_west_workspace
    run_container 'west update --fetch-opt=--filter=tree:0 && west zephyr-export'
    echo "Dependencies are up-to-date."
    ;;

  *)
    echo "Usage: ./build.sh [build|clean|nuke|update]"
    ;;
esac
