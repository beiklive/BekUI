#!/usr/bin/env bash
# 从源码构建 SDL3，产物放到 dist/libSDL3.a，不再依赖系统安装的 SDL3。
#
# 用法：
#   ./scripts/build_sdl3.sh          # 已存在则跳过
#   ./scripts/build_sdl3.sh --force  # 强制重编
#
# 依赖：cmake（brew install cmake）。不需要 brew install sdl3。

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDL_SRC="${ROOT_DIR}/3rd/SDL"
SDL_BUILD="${ROOT_DIR}/build/sdl3"
SDL_OUT="${ROOT_DIR}/dist/libSDL3.a"
# SDL3 的安装前缀：只需要它的 pkg-config 文件（Linux 静态链接要解析系统依赖）。
SDL_PREFIX="${ROOT_DIR}/dist/sdl3-prefix"

FORCE=0
if [[ "${1:-}" == "--force" ]]; then
    FORCE=1
fi

# 源码检查
if [[ ! -f "${SDL_SRC}/include/SDL3/SDL.h" ]]; then
    echo "错误：3rd/SDL 不完整，请先执行 git submodule update --init --recursive" >&2
    exit 1
fi

if [[ -f "${SDL_OUT}" && "${FORCE}" -eq 0 ]]; then
    echo "SDL3 静态库已存在，跳过：${SDL_OUT}"
    exit 0
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "错误：未找到 cmake，请先执行 brew install cmake" >&2
    exit 1
fi

# 打印版本，方便核对
SDL_VERSION="$(grep -E '^#define SDL_(MAJOR|MINOR|MICRO)_VERSION' "${SDL_SRC}/include/SDL3/SDL_version.h" \
    | awk '{print $3}' | paste -sd. -)"
echo "从源码构建 SDL3 ${SDL_VERSION} ..."

mkdir -p "${SDL_BUILD}" "${ROOT_DIR}/dist"

# 基础参数：只构建静态库，关掉测试与示例。
SDL_CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE=Release
    -DSDL_STATIC=ON
    -DSDL_SHARED=OFF
    -DSDL_TESTS=OFF
    -DSDL_EXAMPLES=OFF
    # 打开 install：SDL3 只在 SDL_INSTALL=ON 时才生成 sdl3.pc。
    # 我们装到 dist/sdl3-prefix，并禁用所有可选的 install 组件，只留库与 .pc。
    -DSDL_INSTALL=ON
    -DCMAKE_INSTALL_PREFIX="${SDL_PREFIX}"
    -DSDL_INSTALL_DOCS=OFF
)

# Linux 下 SDL3 有一堆可选的系统依赖（X11 的 xcursor/xi/xfixes/xrandr、
# Wayland、各类音频后端...）。缺哪个就用 OFF 关掉，这样在任何 Linux 上都能编过，
# 而不是要求先装一长串 -dev 包。
if [[ "$(uname -s)" != "Darwin" ]]; then
    # X11 的一堆扩展都是可选增强，缺哪个就关哪个，不影响基本运行。
    # 列表来自 SDL3 的 CMakeLists.txt。
    for ext in XCURSOR XDBE XFIXES XINPUT XRANDR XSCRNSAVER XSHAPE XSYNC XTEST; do
        SDL_CMAKE_ARGS+=("-DSDL_X11_${ext}=OFF")
    done
    # X11 仍开着（否则没有窗口系统），共享库版本也不需要。
    SDL_CMAKE_ARGS+=(-DSDL_X11_SHARED=OFF -DSDL_INSTALL_TESTS=OFF)

    # 音频后端：有对应的开发包就编，没有就关掉
    for backend in ALSA PULSEAUDIO PIPEWIRE JACK SNDIO; do
        pkg=""
        case "${backend}" in
            ALSA)       pkg="alsa" ;;
            PULSEAUDIO) pkg="libpulse" ;;
            PIPEWIRE)   pkg="libpipewire-0.3" ;;
            JACK)       pkg="jack" ;;
            SNDIO)      pkg="sndio" ;;
        esac

        if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists "${pkg}" 2>/dev/null; then
            SDL_CMAKE_ARGS+=("-DSDL_${backend}=ON")
        else
            SDL_CMAKE_ARGS+=("-DSDL_${backend}=OFF")
        fi
    done

    # 摄像头 / 输入法 / 文档门户等，按需关闭
    for feature in SDL_CAMERA SDL_LIBUSB SDL_IBUS SDL_FCITX SDL_WAYLAND_LIBDECOR; do
        SDL_CMAKE_ARGS+=("-D${feature}=OFF")
    done

    # Wayland 的协议头由 SDL 自带，检测失败就自动关，不用手动处理。
fi

cmake -S "${SDL_SRC}" -B "${SDL_BUILD}" \
    "${SDL_CMAKE_ARGS[@]}" \
    > /dev/null

cmake --build "${SDL_BUILD}" -j "$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

# 装到 dist/sdl3-prefix：拿到 libSDL3.a 与 sdl3.pc
rm -rf "${SDL_PREFIX}"
cmake --install "${SDL_BUILD}" > /dev/null 2>&1 || true

# libSDL3.a：优先用安装产物，没有就从构建目录找
LIBSDL="$(find "${SDL_PREFIX}" -name 'libSDL3.a' -type f | head -1)"
if [[ -z "${LIBSDL}" ]]; then
    LIBSDL="$(find "${SDL_BUILD}" -name 'libSDL3.a' -type f | head -1)"
fi

if [[ -z "${LIBSDL}" ]]; then
    echo "错误：构建完成但没找到 libSDL3.a" >&2
    exit 1
fi

cp "${LIBSDL}" "${SDL_OUT}"

# sdl3.pc：Linux 静态链接 SDL3 需要一长串系统库（X11 / Wayland / ALSA / ...），
# 交给 pkg-config 解析，免得手写漏项。
PC_FILE="$(find "${SDL_PREFIX}" -name 'sdl3.pc' -type f | head -1)"
if [[ -n "${PC_FILE}" ]]; then
    echo "pkg-config 文件已生成：${PC_FILE}"
else
    echo "警告：没有生成 sdl3.pc，Linux 上可能因缺少系统库而链接失败" >&2
fi

echo "SDL3 静态库已生成：${SDL_OUT}"
