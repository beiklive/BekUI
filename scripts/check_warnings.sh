#!/usr/bin/env bash
# 零警告门禁：从一份构建日志里挑出「我们自己写的代码」的 warning。
#
#   bash scripts/check_warnings.sh <构建日志>    # 有警告就打印出来并退出 1
#
# 为什么不是直接 grep 整个日志：bklv/3rd/ 下全是 vendored 的 submodule（SDL3 / Yoga /
# imgui / webp / lunasvg），它们按自己的 -Wall 编，跟我们给的参数无关却会一起进日志 ——
# Linux 上关掉 X11 扩展之后，SDL 的 x11 后端就会剩两条 unused label / unused variable。
# 零警告约定约束的是 bklv 自己的代码，所以按路径把 3rd/ 下的行滤掉。
#
# 用法（CI 与本地门禁都走它，规则只有这一份）：
#   scripts/ci_build.sh 编译完调用；scripts/check.sh 的两个后端循环里调用；
#   Switch 那条（build_switch.sh 的日志）也在 ci.yml 里调用。
set -uo pipefail

log="${1:-}"
if [ -z "$log" ] || [ ! -f "$log" ]; then
    printf '用法: %s <构建日志>\n' "$0" >&2
    exit 2
fi

# GCC/Clang 是 `file.cpp:12:3: warning: …`，MSVC 是 `file.cpp(12,3): warning C4996: …`。
warn_re='warning( C[0-9]+)?[: ]'
# 兼容 / 与 \（MSYS2 下 CMake 给的是 Windows 路径）。
vendored_re='(^|[\\/])3rd[\\/]'

our_warnings="$(grep -E "$warn_re" "$log" | grep -vE "$vendored_re")"
if [ -n "$our_warnings" ]; then
    printf '%s\n' "$our_warnings" | head -30
    exit 1
fi
exit 0
