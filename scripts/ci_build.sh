#!/usr/bin/env bash
# 单后端构建：配置 → 编译 → 零警告门禁。CI 用它，本地复现 CI 也用它。
#
#   bash scripts/ci_build.sh gl     # -> build/gl,  产物在 dist/
#   bash scripts/ci_build.sh vk     # -> build/vk,  产物在 dist/vk/
#
# 为什么不把命令直接写在 ci.yml 里：Windows(MSYS2) / Linux / macOS 三条路径的参数
# 完全一样，写成一份才能保证它们真的按同一套参数构建；"构建失败 / 出现警告就算失败"
# 的门禁逻辑也只出现在这一个地方。
#
# 生成器默认不写死：新目录由 CMake 按 PATH 里的工具链自己挑（Linux/macOS 就是
# Unix Makefiles），已有的构建目录沿用缓存里的生成器。需要指定就设环境变量
# BKLV_CMAKE_GENERATOR：MSYS2 里必须给 Ninja（否则 CMake 会挑机器上装的
# Visual Studio，用 cl.exe 编一份 MinGW 源码），MSVC 那条 CI 给
# "Visual Studio 17 2022"。还能给 "Unix Makefiles"、"Xcode" 之类。
#
# 门禁用 grep 而不是 `| grep -c`：并行构建失败时计数会是 0，被当成成功（踩过）。
# 警告的判定规则在 scripts/check_warnings.sh 里（本地 check.sh、CI 的 Switch 那条共用）。
set -uo pipefail

backend="${1:-}"
case "$backend" in
    gl|vk) ;;
    *)
        printf '用法: %s <gl|vk>\n' "$0" >&2
        exit 2
        ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

dir="${BKLV_BUILD_DIR:-build/$backend}"
jobs="${BKLV_JOBS:-4}"
log="$dir/build.log"

generator_args=()
if [ -n "${BKLV_CMAKE_GENERATOR:-}" ]; then
    generator_args=(-G "$BKLV_CMAKE_GENERATOR")
fi

mkdir -p "$dir"

printf '=== 配置 %s（%s）===\n' "$backend" "$dir"
# ${arr[@]+"${arr[@]}"} 这个写法是为了 macOS 自带的 bash 3.2：那里 set -u 下
# 展开空数组会报 unbound variable。
if ! cmake -S . -B "$dir" ${generator_args[@]+"${generator_args[@]}"} \
        -DBKLV_GRAPHICS="$backend" \
        -DCMAKE_BUILD_TYPE=Release; then
    echo "::error::configure 失败（$backend）"
    exit 1
fi

printf '=== 构建 %s（并行 %s）===\n' "$backend" "$jobs"
# --config Release 对单配置生成器是空操作；不写的话多配置生成器（Visual Studio）
# 会编 Debug，而产物目录是共用的，会盖掉 Release 的二进制。
if ! cmake --build "$dir" --config Release --parallel "$jobs" > "$log" 2>&1; then
    echo "::error::构建失败（$backend）"
    grep -E "error( C[0-9]+)?[: ]" "$log" | head -30
    printf '\n--- 日志末尾 ---\n'
    tail -40 "$log"
    exit 1
fi

# 零警告门禁的规则在 scripts/check_warnings.sh 里（只数 bklv 自己的代码，vendored 的
# 3rd/ 不算），本地 check.sh 与 Switch 那条用的也是同一份。
if ! warnings="$(bash "$ROOT/scripts/check_warnings.sh" "$log")"; then
    echo "::error::构建出现警告（约定是零警告）"
    printf '%s\n' "$warnings"
    exit 1
fi

echo "构建通过，零警告"
