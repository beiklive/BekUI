#!/usr/bin/env bash
# 用 devkitA64 把 demo 编成 Switch 的 .nro。
#
# 用法：
#   ./scripts/build_switch.sh              # 编译
#   ./scripts/build_switch.sh --clean      # 丢掉中间产物重新编译
#
# 依赖：devkitPro（devkitA64 + libnx）。不需要 SDL3：
# SDL3 上游没有 Switch 后端，所以 Switch 上直接用 libnx + EGL/GLES3。
#
# 为什么 Switch 不用 CMake：工具链、编译参数、链接库都和桌面完全不同，
# 而且不能链 SDL3。CMake 覆盖不了，所以这是仓库里唯一保留的构建脚本。
# 源文件列表按目录自动收集，只在"Switch 只取其中一个子集"的地方显式列举，
# 避免和 CMakeLists.txt 手抄两份、加文件时漏掉。

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BKLV_DIR="${ROOT_DIR}/bklv"
THIRD_DIR="${BKLV_DIR}/3rd"

CLEAN=0
for arg in "$@"; do
    case "${arg}" in
        --clean|--force) CLEAN=1 ;;
        --no-webp) WITH_WEBP=0 ;;
        --svg) WITH_SVG=1 ;;
        --no-svg) WITH_SVG=0 ;;
        *) echo "未知参数：${arg}（支持 --clean / --no-webp / --svg / --no-svg）" >&2; exit 1 ;;
    esac
done

# 可选图片解码器。
# WebP：子模块存在就默认开。
# SVG：**Switch 上默认关** —— lunasvg 的矢量后端 plutovg 用了 sys/mman.h，devkitA64 没有这个头，
#      编不过（踩过：加了 SVG 解码器之后 Switch 构建直接断在这里）。
#      桌面仍然默认开；Switch 上确实要用就显式 --svg，并且得自己解决 mmap 缺失。
WITH_WEBP="${WITH_WEBP:-1}"
WITH_SVG="${WITH_SVG:-0}"

# ---- 工具链 ----
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
DEVKITA64="${DEVKITPRO}/devkitA64"
PORTLIBS="${DEVKITPRO}/portlibs/switch"
LIBNX="${DEVKITPRO}/libnx"
CC="${DEVKITA64}/bin/aarch64-none-elf-gcc"
CXX="${DEVKITA64}/bin/aarch64-none-elf-g++"
ELF2NRO="${DEVKITPRO}/tools/bin/elf2nro"
BUILD_ROMFS="${DEVKITPRO}/tools/bin/build_romfs"
STRIP="${DEVKITA64}/bin/aarch64-none-elf-strip"

if [[ ! -x "${CXX}" ]]; then
    echo "错误：未找到 devkitA64 工具链（${CXX}）" >&2
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

# BKLV_SWITCH 打开 Switch 相关代码；BKLV_GLES3 让 ImGui 的 OpenGL3 后端按 ES3 编，
# 并让 bklv 的 GL 头走 gles3_loader.hpp。
# BKLV_USE_STB_IMAGE：Switch 用 stb_image 解码图片（macOS 才用系统 ImageIO）。
# 关掉 ImGui 的 shell 执行工具：它引用了 Switch 上没有的 waitpid/execvp。
# __SWITCH__：devkitPro 的 EGL 头靠它选择平台分支，必须显式定义。
DEFINES=(-D__SWITCH__ -DBKLV_SWITCH -DBKLV_GLES3 -DBKLV_USE_STB_IMAGE -DIMGUI_IMPL_OPENGL_ES3
    -DIMGUI_DISABLE_DEBUG_TOOLS
    -DIMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
    -DIMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# -I"${ROOT_DIR}"：让 #include "bklv/..." 生效
# -I"${ROOT_DIR}/demo"：让 demo 的 #include "app/..." 生效
INCLUDES=(-I"${ROOT_DIR}"
    -I"${ROOT_DIR}/demo"
    -I"${THIRD_DIR}/imgui"
    -I"${THIRD_DIR}/yoga"
    -I"${THIRD_DIR}/stb"
    -I"${THIRD_DIR}/tweeny/include"
    -I"${THIRD_DIR}/json/single_include"
    -I"${PORTLIBS}/include"
    -I"${LIBNX}/include")

# 注意：只关 RTTI，**不关异常**。
# 关掉异常会让 C++ 标准库的部分设施不能用（std::filesystem 出错只能 terminate），
# 而 JSON 解析这类"输入不可信"的代码需要可控的失败路径。保留异常后仍约定：
# 库的公开 API 不向外抛异常，失败用返回值 + 日志表达。
# ---- 可选解码器 ----
WEBP_SOURCES=()
if [[ "${WITH_WEBP}" -eq 1 && -d "${THIRD_DIR}/webp/src/dec" ]]; then
    # libwebp 的 config.h 是它自己的 CMake 生成的；这里**不定义 HAVE_CONFIG_H**，
    # 让它走"编译器探测"的默认分支（源码里是 #ifdef HAVE_CONFIG_H 才 include）。
    # 只取解码需要的目录：dec/demux/dsp/utils —— 不带编码器与 sharpyuv，nro 更小。
    DEFINES+=(-DBKLV_USE_WEBP)
    INCLUDES+=(-I"${THIRD_DIR}/webp" -I"${THIRD_DIR}/webp/src")
    while IFS= read -r -d '' f; do
        WEBP_SOURCES+=("$f")
    done < <(find "${THIRD_DIR}/webp/src/dec" "${THIRD_DIR}/webp/src/demux" \
        "${THIRD_DIR}/webp/src/dsp" "${THIRD_DIR}/webp/src/utils" -name '*.c' -print0 | sort -z)
else
    echo "提示：没编 WebP 解码（子模块缺失或 --no-webp）" >&2
fi

SVG_SOURCES=()
if [[ "${WITH_SVG}" -eq 1 && -d "${THIRD_DIR}/lunasvg/source" && -d "${THIRD_DIR}/plutovg/source" ]]; then
    DEFINES+=(-DBKLV_USE_SVG -DLUNASVG_BUILD -DLUNASVG_BUILD_STATIC -DPLUTOVG_BUILD -DPLUTOVG_BUILD_STATIC)
    INCLUDES+=(-I"${THIRD_DIR}/lunasvg/include" -I"${THIRD_DIR}/lunasvg/source"
        -I"${THIRD_DIR}/plutovg/include" -I"${THIRD_DIR}/plutovg/source")
    while IFS= read -r -d '' f; do
        SVG_SOURCES+=("$f")
    done < <(find "${THIRD_DIR}/lunasvg/source" -name '*.cpp' -print0 | sort -z)
    while IFS= read -r -d '' f; do
        SVG_SOURCES+=("$f")
    done < <(find "${THIRD_DIR}/plutovg/source" -name '*.c' -print0 | sort -z)
else
    echo "提示：没编 SVG 支持（子模块缺失或 --no-svg）" >&2
fi

CXXFLAGS=(-std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -fno-rtti
    "${ARCH[@]}" "${DEFINES[@]}" "${INCLUDES[@]}")

# C 源文件：不传 -std=c++17 / -fno-rtti（那是 C++ 的），第三方代码的警告也放开
CFLAGS=(-std=c11 -O2 "${ARCH[@]}" "${DEFINES[@]}" "${INCLUDES[@]}")

# ---- 源文件 ----
# core 与 views 全都要，按目录收集。
BKLV_SOURCES=()
while IFS= read -r -d '' f; do
    BKLV_SOURCES+=("$f")
done < <(find "${BKLV_DIR}/compile" "${BKLV_DIR}/core" "${BKLV_DIR}/glass" "${BKLV_DIR}/views" "${BKLV_DIR}/widgets" -maxdepth 1 -name '*.cpp' -print0 | sort -z)

# backends / platforms 里 Switch 只取这几个：
# 桌面的 opengl/vulkan/sdl3 那几份不参与（文件内部也有 BKLV_SWITCH 守卫，但不必编）。
BKLV_SOURCES+=(
    "${BKLV_DIR}/backends/gles3_backend.cpp"
    # 玻璃的 GL 后端（桌面 GL 与 Switch GLES3 共用同一份实现与着色器）
    "${BKLV_DIR}/backends/gl_glass.cpp"
    "${BKLV_DIR}/backends/gl_glass_procs.cpp"
    "${BKLV_DIR}/backends/gl_glass_shaders.cpp"
    "${BKLV_DIR}/backends/imgui_impl_libnx.cpp"
    "${BKLV_DIR}/platforms/switch_platform.cpp"
    "${BKLV_DIR}/platforms/switch_ime.cpp"
    "${BKLV_DIR}/platforms/switch_gamepad.cpp"
)

IMGUI_SOURCES=(
    "${THIRD_DIR}/imgui/imgui.cpp"
    "${THIRD_DIR}/imgui/imgui_draw.cpp"
    "${THIRD_DIR}/imgui/imgui_tables.cpp"
    "${THIRD_DIR}/imgui/imgui_widgets.cpp"
    "${BKLV_DIR}/backends/imgui_impl_opengl3_wrapper.cpp"
)

# demo：主程序 + app 层（页面 / tab / 列表项）
DEMO_SOURCES=("${ROOT_DIR}/demo/main.cpp")
while IFS= read -r -d '' f; do
    DEMO_SOURCES+=("$f")
done < <(find "${ROOT_DIR}/demo/app" "${ROOT_DIR}/demo/contrib" -name '*.cpp' -print0 | sort -z)

# Yoga 源码（与桌面一致）
YOGA_SOURCES=()
while IFS= read -r -d '' f; do
    YOGA_SOURCES+=("$f")
done < <(find "${THIRD_DIR}/yoga/yoga" -type f -name '*.cpp' -print0)

echo "编译 demo (Switch / devkitA64) ..."

# 逐个编译成 .o，避免一次调用太多文件。
OBJS=()
# 是否需要重编。**必须连头文件一起比**：只看 .cpp 的话，改头文件不会触发重编，
# 会链进旧的目标文件——踩过一次：App 的默认平台改成了分平台的 SWITCH，
# 但 app.o 没重编，nro 里还是旧值，现象是"改了代码重新编译，行为一点没变"。
# 这里用 -MMD -MP 生成 make 格式的依赖文件，再逐个比其中头文件的时间戳。
needs_rebuild() {
    local src="$1" obj="$2" dep="$3"

    if [[ ! -f "${obj}" || ! -f "${dep}" ]]; then
        return 0
    fi
    if [[ "${src}" -nt "${obj}" ]]; then
        return 0
    fi

    local prereq
    for prereq in $(tr '\\\n' '  ' < "${dep}" | sed 's/^[^:]*://'); do
        if [[ -f "${prereq}" && "${prereq}" -nt "${obj}" ]]; then
            return 0
        fi
    done
    return 1
}

compile_one() {
    local src="$1"
    local rel="${src#"${ROOT_DIR}/"}"
    local obj="${BUILD_DIR}/${rel%.cpp}.o"
    local dep="${obj%.o}.d"
    mkdir -p "$(dirname "${obj}")"
    if needs_rebuild "${src}" "${obj}" "${dep}"; then
        "${CXX}" "${CXXFLAGS[@]}" -MMD -MP -c "${src}" -o "${obj}"
    fi
    OBJS+=("${obj}")
}

# C 源文件（libwebp / plutovg）必须用 C 编译器：拿 g++ 当 C++ 编会挂
compile_c_one() {
    local src="$1"
    local rel="${src#"${ROOT_DIR}/"}"
    local obj="${BUILD_DIR}/${rel%.c}.o"
    local dep="${obj%.o}.d"
    mkdir -p "$(dirname "${obj}")"
    if needs_rebuild "${src}" "${obj}" "${dep}"; then
        "${CC}" "${CFLAGS[@]}" -MMD -MP -c "${src}" -o "${obj}"
    fi
    OBJS+=("${obj}")
}

for src in "${BKLV_SOURCES[@]}" "${IMGUI_SOURCES[@]}" "${DEMO_SOURCES[@]}"; do
    compile_one "${src}"
done

for src in "${WEBP_SOURCES[@]}" "${SVG_SOURCES[@]}"; do
    case "${src}" in
        *.c) compile_c_one "${src}" ;;
        *) compile_one "${src}" ;;
    esac
done

# Yoga 需要 C++20（与桌面构建一致），其余代码保持 C++17。
YOGA_FLAGS=("${CXXFLAGS[@]}")
YOGA_FLAGS=("${YOGA_FLAGS[@]/#-std=c++17/-std=c++20}")

for src in "${YOGA_SOURCES[@]}"; do
    rel="${src#"${ROOT_DIR}/"}"
    obj="${BUILD_DIR}/${rel%.cpp}.o"
    dep="${obj%.o}.d"
    mkdir -p "$(dirname "${obj}")"
    if needs_rebuild "${src}" "${obj}" "${dep}"; then
        "${CXX}" "${YOGA_FLAGS[@]}" -MMD -MP -c "${src}" -o "${obj}"
    fi
    OBJS+=("${obj}")
done

# ---- 链接 ----
# 必须带 switch.specs，否则会链到主机的 crt0 并报 _start 重复定义。
LDFLAGS=(-specs="${LIBNX}/switch.specs" "${ARCH[@]}" -Wl,-Map,"${BUILD_DIR}/link.map")
# 顺序与 devkitPro 的 OpenGL 示例一致：libEGL 依赖 glapi 与 drm_nouveau。
LIBS=(-L"${PORTLIBS}/lib" -L"${LIBNX}/lib" -lEGL -lglapi -ldrm_nouveau -lGLESv2 -lnx -lm)

ELF="${OUT_DIR}/demo.elf"
NRO="${OUT_DIR}/demo.nro"

"${CXX}" "${OBJS[@]}" "${LDFLAGS[@]}" "${LIBS[@]}" -o "${ELF}"

# 去掉调试符号：不 strip 的话 nro 会大出好几 MB（main.elf 保留符号便于调试）。
ELF_STRIPPED="${BUILD_DIR}/demo.stripped.elf"
cp "${ELF}" "${ELF_STRIPPED}"
if [[ -x "${STRIP}" ]]; then
    "${STRIP}" --strip-all "${ELF_STRIPPED}"
fi

# ---- 资源打进 romfs ----
#
# 分两步：build_romfs 生成镜像，elf2nro --romfs 嵌进去。
#
# 为什么不用 elf2nro 的 --romfsdir：devkitPro 的例子是那么写的，但这个工具链里的
# elf2nro 会**静默忽略** --romfsdir（参数能解析——同时给 --romfs 和 --romfsdir 会报
# "Cannot have a RomFS and a RomFS Directory at the same time"——但产物和不带参数时
# 逐字节相同，NRO 里既没有 ASET 段也没有 IVFC）。用两步法产物正确：
# ASET 段存在，romfs 内容与镜像 sha256 一致。
#
# staging 目录里保留 resources/ 这一层，使 romfs 内路径（romfs:/resources/xxx）
# 与桌面的相对路径（resources/xxx）完全一致。
# 运行时由 romfsInit() 把 nro 里的 romfs 挂成 romfs:/。
ROMFS_DIR="${BUILD_DIR}/romfs"
ROMFS_IMAGE="${BUILD_DIR}/romfs.img"
rm -rf "${ROMFS_DIR}" "${ROMFS_IMAGE}"
mkdir -p "${ROMFS_DIR}"
ELF2NRO_ARGS=()
if [[ -d "${ROOT_DIR}/resources" ]]; then
    cp -R "${ROOT_DIR}/resources" "${ROMFS_DIR}/resources"
    "${BUILD_ROMFS}" "${ROMFS_DIR}" "${ROMFS_IMAGE}"
    ELF2NRO_ARGS+=(--romfs="${ROMFS_IMAGE}")
    echo "romfs：resources/ 已打进镜像（$(find "${ROOT_DIR}/resources" -type f | wc -l | tr -d ' ') 个文件，镜像 $(stat -f%z "${ROMFS_IMAGE}" 2>/dev/null || stat -c%s "${ROMFS_IMAGE}") 字节）"
else
    echo "警告：没有 resources/ 目录，nro 里将不带资源" >&2
fi

# ---- 打包成 nro ----
"${ELF2NRO}" "${ELF_STRIPPED}" "${NRO}" "${ELF2NRO_ARGS[@]}"

echo "Switch 产物：${NRO}  ($(du -h "${NRO}" | cut -f1))"
echo "推送到机器：nxlink -s ${NRO}"
