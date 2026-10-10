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

# 只构建静态库，关掉测试，减少编译量与依赖。
# SDL 的音频/视频子系统在 macOS 上需要 Objective-C，CMake 会自动处理 .m 文件。
cmake -S "${SDL_SRC}" -B "${SDL_BUILD}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSDL_STATIC=ON \
    -DSDL_SHARED=OFF \
    -DSDL_TESTS=OFF \
    -DSDL_EXAMPLES=OFF \
    -DSDL_INSTALL=OFF \
    > /dev/null

cmake --build "${SDL_BUILD}" --target SDL3-static -j "$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

# 产物位置在不同 SDL 版本里略有差异，这里都找一遍
LIBSDL="$(find "${SDL_BUILD}" -name 'libSDL3.a' -type f | head -1)"
if [[ -z "${LIBSDL}" ]]; then
    echo "错误：构建完成但没找到 libSDL3.a" >&2
    exit 1
fi

cp "${LIBSDL}" "${SDL_OUT}"
echo "SDL3 静态库已生成：${SDL_OUT}"
