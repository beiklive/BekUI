#!/usr/bin/env bash
# 一次跑完该跑的全部检查：命名规范 → 分层依赖方向 → GL/VK 双后端零警告构建。
#
# 存在的理由：以前用 `cmake --build ... | grep -c " error|warning:"` 判断构建结果，
# 并行构建失败时（例如资源复制的竞态）grep 数到 0，就被当成"构建成功"，
# 结果验证的是**旧二进制**。这里所有步骤都以退出码为准，失败立刻停。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# MSYS/MinGW 的 profile 会同时导出 ALL_PROXY 与 all_proxy。Windows 的 MSBuild 把环境变量
# 塞进 .NET 的字典里（大小写不敏感），两份同名不同大小写同时进来就撞 key，直接以
# MSB6001「已添加项，字典中的关键字 ALL_PROXY 已添加」失败 —— 构建不需要代理，
# 只在真的出现大小写重复时把全小写那份去掉。
case "$(uname -s)" in
    MSYS* | MINGW*)
        for name in all_proxy http_proxy https_proxy no_proxy; do
            upper="$(printf '%s' "$name" | tr '[:lower:]' '[:upper:]')"
            if [ -n "${!name:-}" ] && [ -n "${!upper:-}" ]; then unset "$name"; fi
        done
        ;;
esac

step() { printf '\n=== %s ===\n' "$1"; }
die() { printf '✗ %s\n' "$1" >&2; exit 1; }

step "接口命名检查"
python3 scripts/check_naming.py || die "命名检查未通过"

# 分层是目录约定（所有层编进同一个 OBJECT 库），编译器管不了，只能靠这个脚本兜住：
# core / views / widgets / compile / glass 之间的反向依赖禁止新增。
step "分层依赖方向检查"
python3 scripts/check_layers.py || die "出现新的反向依赖（改法见脚本头部注释）"

for backend in gl vk; do
    step "构建 $backend"
    dir="build/$backend"
    if [ -f "$dir/CMakeCache.txt" ]; then
        # 已有构建目录就沿用里面的生成器：Windows 上它可能是 Visual Studio，而 PATH 里的
        # cmake 是 MinGW 那套，重新 configure 会以"生成器不匹配"直接失败。
        # 但后端写错过就不能继续 —— 否则 build/vk 里编出来的是 GL 版，还看着像成功。
        configured="$(sed -n 's/^BKLV_GRAPHICS:[^=]*=//p' "$dir/CMakeCache.txt")"
        [ "$configured" = "$backend" ] \
            || die "$dir 里配的是 BKLV_GRAPHICS=$configured（想改就删掉这个目录）"
    else
        cmake -S . -B "$dir" -DBKLV_GRAPHICS=$backend > /dev/null \
            || die "$backend configure 失败"
    fi
    # --config 对单配置生成器（Makefiles / Ninja）是空操作；不写的话多配置生成器
    # （Visual Studio）会编 Debug，而产物目录是共用的，会盖掉 Release 的 exe。
    cmake --build "$dir" --config Release -j 8 > "/tmp/bklv_build_$backend.log" 2>&1 \
        || { grep -E "error( C[0-9]+)?[: ]" "/tmp/bklv_build_$backend.log" | head -10; die "$backend 构建失败"; }
    if grep -qE "warning( C[0-9]+)?[: ]" "/tmp/bklv_build_$backend.log"; then
        grep -E "warning( C[0-9]+)?[: ]" "/tmp/bklv_build_$backend.log" | head -10
        die "$backend 构建有警告（约定是零警告）"
    fi
    echo "构建通过，零警告"
done

printf '\n全部检查通过\n'
