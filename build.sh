#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="zmk-build-local"
VOLUMES=(zmk-zephyr zmk-modules zmk-tools zmk-west)
BOARD="seeeduino_xiao_ble"
SHIELD="revxlp30"
KEYMAP="app/snippets/fj88/k30.keymap"
KEYMAP_NAME="$(basename "${KEYMAP%.keymap}")"

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

PRISTINE=""
if [[ "${2:-}" == "-p" || "${1:-}" == "-p" ]]; then
  PRISTINE="-p"
fi

case "${1:-build}" in
  build)
    echo "==> Building Docker image..."
    docker build -t "$IMAGE_NAME" -f Dockerfile.build .

    run_container '
      if [ ! -f .west/config ]; then
        echo "==> Initial setup: Running west init and update..."
        printf "[manifest]\npath = app\nfile = west.yml\n" > .west/config
        west update
        west zephyr-export
      fi
      west build -s app -d app/build '"$PRISTINE"' -b '"$BOARD"' -- -DSHIELD='"$SHIELD"'
    '

    mkdir -p .build
    cp app/build/zephyr/zmk.uf2 ".build/${SHIELD}-${BOARD}.uf2"
    echo "Artifact: .build/${SHIELD}-${BOARD}.uf2"

    # Generate keymap SVG
    run_container '
      KM=/tmp/k30.keymap
      sed -e "s/&je /\&kp /g" -e "s/&jmt /\&mt /g" -e "s/&jlt /\&lt /g" \
          -e "s/JP_COLN/COLON/g" -e "s/JP_RT/GT/g" -e "s/JP_//g" \
        '"$KEYMAP"' > "$KM"
      CFG=app/snippets/fj88/keymap-drawer/config.yaml
      keymap -c "$CFG" parse -z "$KM" -l BASE JPN PAD SYM FUNC MSE OPT \
        | sed "1s|^layout:.*|layout: {cols_thumbs_notation: 2vv3332+2 2+23332vv}|" \
        > app/build/keymap.yaml
      keymap -c "$CFG" draw app/build/keymap.yaml -s BASE PAD SYM --keys-only -o app/build/keymap.svg
      keymap -c "$CFG" draw app/build/keymap.yaml -s BASE PAD SYM --combos-only -o app/build/keymap-combos.svg
    '
    cp app/build/keymap.yaml ".build/${KEYMAP_NAME}.yaml"
    cp app/build/keymap.svg ".build/${KEYMAP_NAME}.svg"
    cp app/build/keymap-combos.svg ".build/${KEYMAP_NAME}-combos.svg"
    echo "Keymap:   .build/${KEYMAP_NAME}.yaml .build/${KEYMAP_NAME}.svg .build/${KEYMAP_NAME}-combos.svg"
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
    echo "==> Building Docker image..."
    docker build -t "$IMAGE_NAME" -f Dockerfile.build .
    run_container 'west update && west zephyr-export'
    echo "Dependencies are up-to-date."
    ;;

  *)
    echo "Usage: ./build.sh [build|clean|nuke|update]"
    ;;
esac
