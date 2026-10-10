#!/usr/bin/env bash
# 使用严格模式，让脚本在命令失败或变量未定义时立即停止。
set -euo pipefail

# 取得脚本所在目录，确保从任意工作目录执行都能找到项目文件。
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 允许用户通过环境变量替换编译器，默认使用 macOS 的 clang++。
CXX="${CXX:-clang++}"
# SDL3 从源码构建（3rd/SDL 子模块），不使用系统安装的版本。
SDL_SRC="${ROOT_DIR}/3rd/SDL"
SDL_LIB="${ROOT_DIR}/dist/libSDL3.a"
if [[ ! -f "${SDL_SRC}/include/SDL3/SDL.h" ]]; then
    echo "错误：3rd/SDL 不完整，请先执行 git submodule update --init --recursive" >&2
    exit 1
fi
# 静态库不存在就先编译一份（脚本内部会跳过已存在的情况）。
if [[ ! -f "${SDL_LIB}" ]]; then
    "${ROOT_DIR}/scripts/build_sdl3.sh"
fi
# 指定 C++ 标准、警告选项以及 Dear ImGui、SDL3、Yoga 和 Tweeny 头文件目录。
CXXFLAGS=(-Wall -Wextra -Wpedantic -DGL_SILENCE_DEPRECATION -I"${ROOT_DIR}/3rd/imgui" -I"${ROOT_DIR}/3rd/yoga" -I"${ROOT_DIR}/3rd/tweeny/include" -I"${ROOT_DIR}/src" -I"${SDL_SRC}/include")
# 指定 Dear ImGui 核心实现及 SDL3/OpenGL3 平台、渲染后端实现文件。
IMGUI_SOURCES=(
    "${ROOT_DIR}/3rd/imgui/imgui.cpp"
    "${ROOT_DIR}/3rd/imgui/imgui_draw.cpp"
    "${ROOT_DIR}/3rd/imgui/imgui_tables.cpp"
    "${ROOT_DIR}/3rd/imgui/imgui_widgets.cpp"
    "${ROOT_DIR}/3rd/imgui/backends/imgui_impl_sdl3.cpp"
    "${ROOT_DIR}/3rd/imgui/backends/imgui_impl_opengl3.cpp"
)
# 指定 bklv 移植层的实现文件，它们会被编译成目标文件供各示例复用。
BKLV_SOURCES=(
    "${ROOT_DIR}/src/bklv/core/logger.cpp"
    "${ROOT_DIR}/src/bklv/core/input.cpp"
    "${ROOT_DIR}/src/bklv/core/theme.cpp"
    "${ROOT_DIR}/src/bklv/core/time.cpp"
    "${ROOT_DIR}/src/bklv/core/font.cpp"
    "${ROOT_DIR}/src/bklv/core/graphics.cpp"
    "${ROOT_DIR}/src/bklv/core/platform.cpp"
    "${ROOT_DIR}/src/bklv/core/gamepad.cpp"
    "${ROOT_DIR}/src/bklv/core/view.cpp"
    "${ROOT_DIR}/src/bklv/core/box.cpp"
    "${ROOT_DIR}/src/bklv/core/app.cpp"
    "${ROOT_DIR}/src/bklv/views/label.cpp"
    "${ROOT_DIR}/src/bklv/views/scrolling_frame.cpp"
    "${ROOT_DIR}/src/bklv/views/h_scrolling_frame.cpp"
    "${ROOT_DIR}/src/bklv/views/recycler.cpp"
    "${ROOT_DIR}/src/bklv/views/image.cpp"
    "${ROOT_DIR}/src/bklv/views/progress_spinner.cpp"
    "${ROOT_DIR}/src/bklv/platforms/sdl3_platform.cpp"
    "${ROOT_DIR}/src/bklv/platforms/sdl3_gamepad.cpp"
    "${ROOT_DIR}/src/bklv/backends/opengl_backend.cpp"
    "${ROOT_DIR}/src/bklv/backends/null_backend.cpp"
    "${ROOT_DIR}/src/bklv/views/hints.cpp"
    "${ROOT_DIR}/src/bklv/views/debug_layer.cpp"
)
# 指定所有需要一次性编译的学习程序。
# 第四列留空表示该示例不需要链接 bklv 移植层，填 1 表示需要。
TARGETS=(
    "demo_01_widgets:${ROOT_DIR}/demo/demo_01_widgets.cpp:"
    "demo_02_layout:${ROOT_DIR}/demo/demo_02_layout.cpp:"
    "demo_03_custom_widget:${ROOT_DIR}/demo/demo_03_custom_widget.cpp:"
    "demo_04_yoga:${ROOT_DIR}/demo/demo_04_yoga.cpp:"
    "demo_05_tweeny:${ROOT_DIR}/demo/demo_05_tweeny.cpp:"
    "demo_06_bklv_layout:${ROOT_DIR}/demo/demo_06_bklv_layout.cpp:1"
    "demo_07_bklv_showcase:${ROOT_DIR}/demo/demo_07_bklv_showcase.cpp:1"
    "imgui_main:${ROOT_DIR}/src/main.cpp:"
)

# 检查 ImGui 子模块是否已经初始化。
if [[ ! -f "${ROOT_DIR}/3rd/imgui/imgui.h" ]]; then
    echo "错误：3rd/imgui 不完整，请先执行 git submodule update --init --recursive" >&2
    exit 1
fi
# 检查 Yoga 子模块是否已经初始化。
if [[ ! -f "${ROOT_DIR}/3rd/yoga/yoga/Yoga.h" ]]; then
    echo "错误：3rd/yoga 不完整，请先执行 git submodule update --init --recursive" >&2
    exit 1
fi
# 检查 Tweeny 子模块是否已经初始化。
if [[ ! -f "${ROOT_DIR}/3rd/tweeny/include/tweeny/tweeny.h" ]]; then
    echo "错误：3rd/tweeny 不完整，请先执行 git submodule update --init --recursive" >&2
    exit 1
fi

# 创建构建输出目录；已有其他文件会保留。
mkdir -p "${ROOT_DIR}/dist"

# 编译 Yoga 的所有核心 C++ 源文件，保存中间目标文件供后续重编使用。
YOGA_OBJECTS=()
while IFS= read -r -d '' source; do
    relative_path="${source#"${ROOT_DIR}/3rd/yoga/"}"
    object="${ROOT_DIR}/dist/yoga-objects/${relative_path%.cpp}.o"
    mkdir -p "$(dirname "${object}")"
    "${CXX}" -std=c++20 -O2 -I"${ROOT_DIR}/3rd/yoga" -c "${source}" -o "${object}"
    YOGA_OBJECTS+=("${object}")
done < <(find "${ROOT_DIR}/3rd/yoga/yoga" -type f -name '*.cpp' -print0)
# 将 Yoga 目标文件打包为供示例链接的静态库。
libtool -static -o "${ROOT_DIR}/dist/libyogacore.a" "${YOGA_OBJECTS[@]}"

# 编译 bklv 移植层：每个文件一个目标文件，避免每个示例重复编译。
BKLV_OBJECTS=()
for source in "${BKLV_SOURCES[@]}"; do
    relative_path="${source#"${ROOT_DIR}/src/"}"
    object="${ROOT_DIR}/dist/bklv-objects/${relative_path%.cpp}.o"
    mkdir -p "$(dirname "${object}")"
    "${CXX}" -std=c++17 -O2 "${CXXFLAGS[@]}" -c "${source}" -o "${object}"
    BKLV_OBJECTS+=("${object}")
done

# 链接参数：SDL3 静态库 + 它依赖的 macOS framework + OpenGL 与图片解码。
# framework 列表取自 SDL3 的 sdl3.pc 的 Libs.private。
LINK_FLAGS=(
    "${SDL_LIB}"
    -framework OpenGL
    -framework ImageIO
    -framework CoreGraphics
    -framework CoreFoundation
    -framework CoreMedia
    -framework CoreVideo
    -framework Cocoa
    -weak_framework UniformTypeIdentifiers
    -framework IOKit
    -weak_framework ForceFeedback
    -framework Carbon
    -framework CoreAudio
    -framework AudioToolbox
    -framework AVFoundation
    -framework Foundation
    -framework GameController
    -framework Metal
    -framework QuartzCore
    -weak_framework CoreHaptics
    -lpthread
    -lm
)
# 逐个编译目标程序，并把可执行文件放进 dist 目录。
for target in "${TARGETS[@]}"; do
    name="${target%%:*}"
    rest="${target#*:}"
    source="${rest%%:*}"
    use_bklv="${rest#*:}"
    echo "编译 ${name} ..."
    EXTRA_LINK=()
    if [[ "${use_bklv}" == "1" ]]; then
        EXTRA_LINK=("${BKLV_OBJECTS[@]}")
    fi
    if [[ "${name}" == "demo_04_yoga" ]]; then
        "${CXX}" -std=c++20 "${CXXFLAGS[@]}" "${source}" "${IMGUI_SOURCES[@]}" "${EXTRA_LINK[@]}" "${LINK_FLAGS[@]}" "${ROOT_DIR}/dist/libyogacore.a" -o "${ROOT_DIR}/dist/${name}"
    else
        "${CXX}" -std=c++17 "${CXXFLAGS[@]}" "${source}" "${IMGUI_SOURCES[@]}" "${EXTRA_LINK[@]}" "${LINK_FLAGS[@]}" "${ROOT_DIR}/dist/libyogacore.a" -o "${ROOT_DIR}/dist/${name}"
    fi
done

# 输出构建完成提示。
echo "构建完成：${ROOT_DIR}/dist"
