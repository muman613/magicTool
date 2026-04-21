#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
    cat <<'USAGE'
Usage:
  ./build.sh [target] [build-target] [type]

Arguments:
  target        pico2 | pico2w        default: pico2
  build-target  host | fw | all       default: all
  type          debug | release       default: debug

Examples:
  ./build.sh
  ./build.sh pico2w fw release
  ./build.sh pico2 host debug
USAGE
}

target="${1:-pico2}"
build_target="${2:-all}"
build_type="${3:-debug}"

if [[ "${target}" == "-h" || "${target}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ $# -gt 3 ]]; then
    usage >&2
    exit 2
fi

case "${target}" in
    pico2 | pico2w)
        ;;
    *)
        echo "Invalid target: ${target}" >&2
        usage >&2
        exit 2
        ;;
esac

case "${build_target}" in
    host | fw | all)
        ;;
    *)
        echo "Invalid build target: ${build_target}" >&2
        usage >&2
        exit 2
        ;;
esac

case "${build_type}" in
    debug | release)
        ;;
    *)
        echo "Invalid build type: ${build_type}" >&2
        usage >&2
        exit 2
        ;;
esac

if [[ "${build_target}" == "host" ]]; then
    preset="host-${build_type}"
elif [[ "${build_target}" == "fw" ]]; then
    preset="firmware-${target}-${build_type}"
else
    preset="all-${target}-${build_type}"
fi

cd "${script_dir}"

echo "Configuring preset: ${preset}"
cmake --preset "${preset}"

echo "Building preset: ${preset}"
cmake --build --preset "${preset}"
