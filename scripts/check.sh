#!/usr/bin/env bash
# 一次跑完该跑的全部检查：命名规范 → 分层依赖方向 → GL/VK 构建 → 全部测试。
#
# 存在的理由：以前用 `cmake --build ... | grep -c " error|warning:"` 判断构建结果，
# 并行构建失败时（例如资源复制的竞态）grep 数到 0，就被当成"构建成功"，
# 结果验证的是**旧二进制**。这里所有步骤都以退出码为准，失败立刻停。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

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
    cmake -S . -B "build/$backend" -DBKLV_GRAPHICS=$backend -DBKLV_BUILD_TESTS=ON > /dev/null \
        || die "$backend configure 失败"
    cmake --build "build/$backend" -j 8 > "/tmp/bklv_build_$backend.log" 2>&1 \
        || { grep -E "error:|Error" "/tmp/bklv_build_$backend.log" | head -10; die "$backend 构建失败"; }
    if grep -qE "warning:" "/tmp/bklv_build_$backend.log"; then
        grep -E "warning:" "/tmp/bklv_build_$backend.log" | head -10
        die "$backend 构建有警告（约定是零警告）"
    fi
    echo "构建通过，零警告"
done

step "全部测试（GL）"
total=0
for test_bin in dist/*_test; do
    name="$(basename "$test_bin")"
    out="$("$test_bin" 2>&1)" || { echo "$out" | tail -5; die "$name 失败"; }
    line="$(echo "$out" | tail -1)"
    echo "$line" | grep -q "0 项失败" || { echo "$out" | tail -5; die "$name 有失败项"; }
    n="$(echo "$line" | grep -oE '^[0-9]+')"
    total=$((total + n))
    printf '  %-20s %s\n' "$name" "$line"
done
echo "GL 合计 $total 项检查"

step "全部测试（VK）"
for test_bin in dist/vk/*_test; do
    name="$(basename "$test_bin")"
    out="$("$test_bin" 2>&1)" || die "VK $name 失败"
    echo "$out" | tail -1 | grep -q "0 项失败" || die "VK $name 有失败项"
done
echo "VK 全部通过"

printf '\n全部检查通过（%s 项）\n' "$total"
