#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
    cat <<'USAGE'
Usage:
  ./build.sh [target] [build-target] [hw-version] [type]

Arguments:
  target        pico2 | pico2w        default: pico2
  build-target  host | fw | all       default: all
  hw-version    1 | 2                 default: 1
  type          debug | release       default: debug

Examples:
  ./build.sh
  ./build.sh pico2w fw 2 release
  ./build.sh pico2 host 1 debug
USAGE
}

target="${1:-pico2}"
build_target="${2:-all}"
hw_version="${3:-1}"
build_type="${4:-debug}"

if [[ "${target}" == "-h" || "${target}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ $# -gt 4 ]]; then
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

case "${hw_version}" in
    1 | 2)
        ;;
    *)
        echo "Invalid hardware version: ${hw_version}" >&2
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

configure_args=("--preset" "${preset}")
if [[ "${build_target}" != "host" ]]; then
    configure_args+=("-DMAGICTOOL_HW_VERSION=${hw_version}")
fi

echo "Configuring preset: ${preset}"
if [[ "${build_target}" != "host" ]]; then
    echo "Hardware version: ${hw_version}"
fi
cmake "${configure_args[@]}"

echo "Building preset: ${preset}"
build_jobs="$(nproc)"
cmake --build --preset "${preset}" -- -j "${build_jobs}"

echo
echo "Build complete."

if [[ "${build_target}" != "host" ]]; then
    if [[ "${target}" == "pico2w" ]]; then
        uf2_path="build/${preset}/firmware/magictool_fw_pico2_w.uf2"
    else
        uf2_path="build/${preset}/firmware/magictool_fw_pico2.uf2"
    fi

    echo
    echo "Flash the firmware image:"
    echo "  picotool load -f --vid 0xcafe --pid 0x4000 ${uf2_path}"
    echo
    echo "Or mount the Pico in BOOTSEL mode and copy:"
    echo "  cp ${uf2_path} /media/\$USER/RPI-RP2/"
fi

if [[ "${build_target}" != "fw" ]]; then
    echo
    echo "Install host tools and development artifacts:"
    echo "  sudo cmake --install build/${preset}"
    echo
    echo "For a user-writable install prefix, reconfigure first with:"
    echo "  cmake --preset ${preset} -DCMAKE_INSTALL_PREFIX=\"\$HOME/.local/magictool\""
    echo "  cmake --build --preset ${preset} -- -j \$(nproc)"
    echo "  cmake --install build/${preset}"
fi
