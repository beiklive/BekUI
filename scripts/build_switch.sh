#!/usr/bin/env bash
# 用 devkitA64 把 demo_07 编成 Switch 的 .nro。
#
# 用法：
#   ./scripts/build_switch.sh              # 默认 demo_07
#   ./scripts/build_switch.sh demo_06      # 指定 demo
#   ./scripts/build_switch.sh demo_07 --clean
#
# 依赖：devkitPro（devkitA64 + libnx）。不需要 SDL3：
# SDL3 上游没有 Switch 后端，所以 Switch 上直接用 libnx + EGL/GLES3。

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# ---- 工具链 ----
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
DEVKITA64="${DEVKITPRO}/devkitA64"
PORTLIBS="${DEVKITPRO}/portlibs/switch"
LIBNX="${DEVKITPRO}/libnx"
CXX="${DEVKITA64}/bin/aarch64-none-elf-g++"
ELF2NRO="${DEVKITPRO}/tools/bin/elf2nro"
STRIP="${DEVKITA64}/bin/aarch64-none-elf-strip"

if [[ ! -x "${CXX}" ]]; then
    echo "错误：未找到 devkitA64 工具链（${CXX}）" >&2
    exit 1
fi

# ---- 选 demo ----
DEMO="${1:-demo_07}"
if [[ "${DEMO}" == "--clean" || "${DEMO}" == "--force" ]]; then
    DEMO="demo_07"
    CLEAN=1
else
    CLEAN=0
    [[ "${2:-}" == "--clean" ]] && CLEAN=1
fi

case "${DEMO}" in
    demo_06) DEMO_SRC="${ROOT_DIR}/demo/demo_06_bklv_layout.cpp" ;;
    demo_07) DEMO_SRC="${ROOT_DIR}/demo/demo_07_bklv_showcase.cpp" ;;
    *)       DEMO_SRC="${DEMO}" ;;
esac

if [[ ! -f "${DEMO_SRC}" ]]; then
    echo "错误：找不到 demo 源文件 ${DEMO_SRC}" >&2
    exit 1
fi

OUT_DIR="${ROOT_DIR}/dist/switch"
BUILD_DIR="${ROOT_DIR}/build/switch"
if [[ "${CLEAN}" -eq 1 ]]; then
    rm -rf "${BUILD_DIR}"
fi
mkdir -p "${OUT_DIR}" "${BUILD_DIR}"

# ---- 编译参数 ----
# ARCH 与 devkitPro 模板保持一致，否则链接期会出现 "read-only segment has dynamic relocations"。
# 注意要拆成数组：整串加引号会被 gcc 当成一个参数。
ARCH=(-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE)

# BKLV_SWITCH 打开 Switch 相关代码；BKLV_GLES3 让 ImGui 的 OpenGL3 后端按 ES3 编。
# 关键：ImGui 的 OpenGL3 后端需要指定加载器，桌面用它自带的，
# Switch 用我们自己的 gles3_loader.hpp（直接包含 GLES3/gl3.h）。
# BKLV_NO_IMAGEIO：Switch 上还没有图片解码器（桌面用 macOS ImageIO）。
# IMGUI_DISABLE_DEBUG_TOOLS 与 IMGUI_DISABLE_DEMO_WINDOWS 之外，
# 还要关掉 ImGui 的 shell 执行工具：它引用了 Switch 上没有的 waitpid/execvp。
# __SWITCH__：devkitPro 的 EGL 头靠它选择平台分支，必须显式定义。
DEFINES=(-D__SWITCH__ -DBKLV_SWITCH -DBKLV_GLES3 -DBKLV_NO_IMAGEIO -DIMGUI_IMPL_OPENGL_ES3
    -DIMGUI_DISABLE_DEBUG_TOOLS
    -DIMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
    -DIMGUI_IMPL_OPENGL_LOADER_CUSTOM='<bklv/backends/gles3_loader.hpp>')

INCLUDES=(-I"${ROOT_DIR}/3rd/imgui"
    -I"${ROOT_DIR}/3rd/yoga"
    -I"${ROOT_DIR}/src"
    -I"${ROOT_DIR}/demo"
    -I"${PORTLIBS}/include"
    -I"${LIBNX}/include")

CXXFLAGS=(-std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -fno-rtti -fno-exceptions
    "${ARCH[@]}" "${DEFINES[@]}" "${INCLUDES[@]}")

# ---- 源文件 ----
BKLV_SOURCES=(
    "${ROOT_DIR}/src/bklv/core/logger.cpp"
    "${ROOT_DIR}/src/bklv/core/input.cpp"
    "${ROOT_DIR}/src/bklv/core/gamepad.cpp"
    "${ROOT_DIR}/src/bklv/core/theme.cpp"
    "${ROOT_DIR}/src/bklv/core/time.cpp"
    "${ROOT_DIR}/src/bklv/core/font.cpp"
    "${ROOT_DIR}/src/bklv/core/graphics.cpp"
    "${ROOT_DIR}/src/bklv/core/platform.cpp"
    "${ROOT_DIR}/src/bklv/core/view.cpp"
    "${ROOT_DIR}/src/bklv/core/box.cpp"
    "${ROOT_DIR}/src/bklv/core/app.cpp"
    "${ROOT_DIR}/src/bklv/views/label.cpp"
    "${ROOT_DIR}/src/bklv/views/hints.cpp"
    "${ROOT_DIR}/src/bklv/views/debug_layer.cpp"
    "${ROOT_DIR}/src/bklv/views/scrolling_frame.cpp"
    "${ROOT_DIR}/src/bklv/views/h_scrolling_frame.cpp"
    "${ROOT_DIR}/src/bklv/views/recycler.cpp"
    "${ROOT_DIR}/src/bklv/views/image.cpp"
    "${ROOT_DIR}/src/bklv/views/progress_spinner.cpp"
    "${ROOT_DIR}/src/bklv/backends/null_backend.cpp"
    "${ROOT_DIR}/src/bklv/backends/gles3_backend.cpp"
    "${ROOT_DIR}/src/bklv/backends/imgui_impl_libnx.cpp"
    "${ROOT_DIR}/src/bklv/platforms/switch_platform.cpp"
    "${ROOT_DIR}/src/bklv/platforms/switch_gamepad.cpp"
)

IMGUI_SOURCES=(
    "${ROOT_DIR}/3rd/imgui/imgui.cpp"
    "${ROOT_DIR}/3rd/imgui/imgui_draw.cpp"
    "${ROOT_DIR}/3rd/imgui/imgui_tables.cpp"
    "${ROOT_DIR}/3rd/imgui/imgui_widgets.cpp"
    "${ROOT_DIR}/3rd/imgui/backends/imgui_impl_opengl3.cpp"
)

# Yoga 源码（与桌面一致）
YOGA_SOURCES=()
while IFS= read -r -d '' f; do
    YOGA_SOURCES+=("$f")
done < <(find "${ROOT_DIR}/3rd/yoga/yoga" -type f -name '*.cpp' -print0)

echo "编译 ${DEMO} (Switch / devkitA64) ..."

# 逐个编译成 .o，避免一次调用太多文件。
OBJS=()
compile_one() {
    local src="$1"
    local rel="${src#"${ROOT_DIR}/"}"
    local obj="${BUILD_DIR}/${rel%.cpp}.o"
    mkdir -p "$(dirname "${obj}")"
    if [[ ! -f "${obj}" || "${src}" -nt "${obj}" ]]; then
        "${CXX}" "${CXXFLAGS[@]}" -c "${src}" -o "${obj}"
    fi
    OBJS+=("${obj}")
}

for src in "${BKLV_SOURCES[@]}" "${IMGUI_SOURCES[@]}" "${DEMO_SRC}"; do
    compile_one "${src}"
done

# Yoga 需要 C++20（与桌面构建一致），其余代码保持 C++17。
YOGA_FLAGS=("${CXXFLAGS[@]}")
# 去掉原来的 -std=c++17，换成 c++20
YOGA_FLAGS=("${YOGA_FLAGS[@]/#-std=c++17/-std=c++20}")

for src in "${YOGA_SOURCES[@]}"; do
    rel="${src#"${ROOT_DIR}/"}"
    obj="${BUILD_DIR}/${rel%.cpp}.o"
    mkdir -p "$(dirname "${obj}")"
    if [[ ! -f "${obj}" || "${src}" -nt "${obj}" ]]; then
        "${CXX}" "${YOGA_FLAGS[@]}" -c "${src}" -o "${obj}"
    fi
    OBJS+=("${obj}")
done

# ---- 链接 ----
# 必须带 switch.specs，否则会链到主机的 crt0 并报 _start 重复定义。
LDFLAGS=(-specs="${LIBNX}/switch.specs" "${ARCH[@]}" -Wl,-Map,"${BUILD_DIR}/link.map")
# 顺序与 devkitPro 的 OpenGL 示例一致：libEGL 依赖 glapi 与 drm_nouveau。
LIBS=(-L"${PORTLIBS}/lib" -L"${LIBNX}/lib" -lEGL -lglapi -ldrm_nouveau -lGLESv2 -lnx -lm)

ELF="${OUT_DIR}/${DEMO}.elf"
NRO="${OUT_DIR}/${DEMO}.nro"

"${CXX}" "${OBJS[@]}" "${LDFLAGS[@]}" "${LIBS[@]}" -o "${ELF}"

# 去掉调试符号：不 strip 的话 nro 会大出好几 MB（main.elf 保留符号便于调试）。
ELF_STRIPPED="${BUILD_DIR}/${DEMO}.stripped.elf"
cp "${ELF}" "${ELF_STRIPPED}"
if [[ -x "${STRIP}" ]]; then
    "${STRIP}" --strip-all "${ELF_STRIPPED}"
fi

# ---- 打包成 nro ----
"${ELF2NRO}" "${ELF_STRIPPED}" "${NRO}"

echo "Switch 产物：${NRO}  ($(du -h "${NRO}" | cut -f1))"
echo "推送到机器：nxlink -s ${NRO}"
