#!/usr/bin/env bash
# Launch a named serialized interactive test, or replay a recorded input stream.
#
# These tests are intentionally not headless: they create a real Wayland/Vulkan
# window, wait for the developer, record the fixed-step input/state stream, and
# serialize the result under test-results/.
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LITEKNIGHT_BUILD_DIR:-${ROOT}/build}"
BINARY="${BUILD_DIR}/soul-knight"
NO_BUILD=0
TEST_NAME=""
REPLAY_PATH=""
LIST_ONLY=0

usage() {
    cat <<'USAGE'
Usage: scripts/test-interactive.sh [options]

  --test <name>        run a named interactive scenario
  --replay <file>      replay a serialized test input stream
  --record             explicit recording mode (the default for --test)
  --list               list known scenarios and availability
  --no-build           do not build before launching
  --help, -h           show this help

Implemented scenarios:
  character_all weapon_all enemy_all projectile_all object_all door_all
  combat_live two_weapon energy health save_load

Reconstructed-but-incomplete scenarios (recorded honestly, not claimed done):
  room_transition hub
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --test) TEST_NAME="${2:-}"; shift 2 ;;
        --test=*) TEST_NAME="${1#*=}"; shift ;;
        --replay) REPLAY_PATH="${2:-}"; shift 2 ;;
        --replay=*) REPLAY_PATH="${1#*=}"; shift ;;
        --record) shift ;;
        --list)
            LIST_ONLY=1
            shift ;;
        --no-build) NO_BUILD=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ ${LIST_ONLY} -eq 1 ]]; then
    cat <<'LIST'
character_all             IMPLEMENTED  recovered character inspection/switch
weapon_all                IMPLEMENTED  recovered weapons + projectiles
enemy_all                 IMPLEMENTED  recovered enemy definitions/spawner
projectile_all            IMPLEMENTED  asset-lab projectile inspection
object_all                IMPLEMENTED  textures/sprites/obstacles inspection
door_all                  IMPLEMENTED  recovered door animation inspection
combat_live               IMPLEMENTED  enemy pursuit/contact/damage/reset
two_weapon                IMPLEMENTED  independent primary/secondary slots
energy                    IMPLEMENTED  serialized consume + HUD energy
health                    IMPLEMENTED  player HP/armor contact damage
save_load                 IMPLEMENTED  native XDG save round-trip
test-ui                   IMPLEMENTED  logical 1080p pixel UI/font
test-font                 IMPLEMENTED  pixel_bold metrics/atlas
test-player-animation     IMPLEMENTED  idle/run/dead clips + frame stepping
test-player-obstacle-collision IMPLEMENTED  F4 visual/collision debug
test-weapon-pickup        IMPLEMENTED  E prompt + pickup + replacement
test-no-weapon-render     IMPLEMENTED  empty equipment renders no weapon
test-two-weapons          IMPLEMENTED  two slots and Q switching
test-energy               IMPLEMENTED  live energy consumption
test-hud                  IMPLEMENTED  top-left HP/armor/energy HUD
test-pause                IMPLEMENTED  frozen gameplay + pause modal
test-settings             IMPLEMENTED  fullscreen/debug settings
test-home                 IMPLEMENTED  main menu/home flow
test-living-room          IMPLEMENTED  recovered HeroRoom hub
test-character-select    IMPLEMENTED  character station selector
test-character-catalogue  IMPLEMENTED  character/animation lab
room_transition           IMPLEMENTED  Living Room <-> r1_1 state-preserving swap
test-room-transition      IMPLEMENTED  same as room_transition
hub                       IMPLEMENTED  recovered HeroRoom launch
LIST
    exit 0
fi

if [[ -z "${REPLAY_PATH}" && -z "${TEST_NAME}" ]]; then
    echo "ERROR: pass --test <name> or --replay <file>" >&2
    exit 2
fi
if [[ -n "${REPLAY_PATH}" && -n "${TEST_NAME}" ]]; then
    echo "ERROR: --test and --replay are mutually exclusive" >&2
    exit 2
fi
if [[ -n "${REPLAY_PATH}" ]]; then
    if [[ ! -f "${REPLAY_PATH}" ]]; then
        echo "ERROR: replay file not found: ${REPLAY_PATH}" >&2
        exit 2
    fi
    REPLAY_PATH="$(readlink -f "${REPLAY_PATH}")"
fi

if [[ ! -f "${BUILD_DIR}/assets/scenes/r1_1_visual/scene.json" ]]; then
    echo "ERROR: prepared assets are missing in ${BUILD_DIR}." >&2
    echo "Run ./scripts/prepare-assets.sh first." >&2
    exit 2
fi

if [[ ${NO_BUILD} -eq 0 ]]; then
    cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
    cmake --build "${BUILD_DIR}" -j "$(nproc)" --target soul-knight >/dev/null
fi

if [[ ! -x "${BINARY}" ]]; then
    echo "ERROR: executable missing: ${BINARY}" >&2
    exit 2
fi

export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}"
export LITEKNIGHT_PROJECT_ROOT="${ROOT}"
export LITEKNIGHT_REFERENCE_APK="${ROOT}/original/SoulKnight-2.6.8.apk"

if [[ -n "${REPLAY_PATH}" ]]; then
    exec "${BINARY}" --replay "${REPLAY_PATH}"
fi
exec "${BINARY}" "--interactive-test=${TEST_NAME}"
