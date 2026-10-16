#!/usr/bin/env python3
"""分层依赖方向检查（约定见 AGENTS.md「分层」与 docs/architecture-review.html §2.1）。

    python3 scripts/check_layers.py          # 有新增反向依赖时退出码 1
    python3 scripts/check_layers.py --list   # 打印当前全部跨层依赖（含计数），总是 0

**为什么需要它**：bklv 的 core / views / widgets / compile / backends / platforms
全部编进同一个 OBJECT 库、共用一个 include 根，所以"分层"目前只是目录约定 ——
编译器不会因为 `core/app.cpp` 反向 include 了 `widgets/` 而报错。
命名规范有 check_naming.py 兜着，"依赖方向"这一条一直没有对应的检查，于是它只能靠评审维持。
这个脚本把约定变成门禁：**禁止新增反向依赖**，存量（下面 BASELINE）先记账，谁消解谁删行。

禁止的方向（左侧的层不许 include 右侧的层）：

    core    → widgets / views / compile / backends
    compile → views / widgets
    views   → widgets

允许的方向（不在检查范围，属正常分层）：

    widgets → views / core / compile              视图与外观驱动的控件在下游
    views → core                                  语义控件只认 ImDrawList 与 core 接口
    compile → core                                解析实现用到 core 的数据类型
    backends → core                               后端实现 core 的接口（含 core/glass）
    platforms → core / backends                   平台创建并持有后端
    core → platforms                              工厂：方向由"编译期裁剪 + 工厂"反转，属预期
    任何一层 → 3rd                                第三方是叶子

注意"层"是 `bklv/` 下的第一级目录：玻璃子系统在 `core/glass/`，算 core 而不是独立一层
（它曾经在根路径的 `glass/` 下，那次搬家把 `core → glass` 与 `glass → backends` 两条
同层/反向依赖一起消化掉了）。

另外还查一条伞头可达范围（见 `check_umbrella()`）：使用者的入口 `bklv/bklv.hpp`
不许间接碰到 `backends/` / `platforms/` 或 `<vulkan/vulkan.h>` —— 曾经
`bklv.hpp → shell.hpp → glass_service.hpp → vk_glass.hpp → <vulkan/vulkan.h>` 让每个
包含伞头的翻译单元都被拖进 Vulkan 头（评审 §4.2 P1）。
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIB = ROOT / "bklv"

# 被禁止的反向依赖：(源层, 目标层)
FORBIDDEN = {
    ("core", "widgets"),
    ("core", "views"),
    ("core", "compile"),
    ("core", "backends"),
    ("compile", "views"),
    ("compile", "widgets"),
    ("views", "widgets"),
}

# 存量白名单：(源层, 目标层, 相对仓库路径)。
# 这些是已经存在、且**评审时明确记录过**的反向依赖；消解之后要把对应行删掉，
# 留着不删这个脚本会报"白名单已过期"（和 CMake 源清单漏项检查同一个取向：
# 记账必须精确，否则白名单会慢慢变成摆设）。
BASELINE = {
    # App 是组合根：弹层（通知）、调试层、视图类型注册、默认字体都挂在这里。
    ("core", "widgets", "bklv/core/app.cpp"),
    ("core", "views", "bklv/core/app.cpp"),
    # 外观的内存类型在 core、解析实现在 compile —— 两个模块互相 include（模块级环）。
    ("core", "compile", "bklv/core/appearance.cpp"),
    # 解析层为了注册 8 个内置视图类型而认识具体 views 实现。
    ("compile", "views", "bklv/compile/view_parse.cpp"),
}

# 伞头（使用者入口）不许间接碰到的目录 / 头文件
UMBRELLA = ROOT / "bklv" / "bklv.hpp"
UMBRELLA_FORBIDDEN_DIRS = ("backends", "platforms")
UMBRELLA_FORBIDDEN_HEADERS = ("<vulkan/vulkan.h>", "vulkan/vulkan.h")

INCLUDE_RE = re.compile(r'#include\s+"(bklv/[^"]+)"')


def layer_of(rel_posix):
    """'core/app.cpp' -> 'core'；直接在 bklv/ 下的文件（伞头）返回 '.'。"""
    parts = rel_posix.split("/")
    return parts[0] if len(parts) > 1 else "."


def collect():
    """返回 {(源层, 目标层, 相对路径): [被 include 的目标文件...]} 与跨层计数。"""
    edges = {}
    counts = {}
    files = sorted(
        p
        for p in LIB.rglob("*")
        if p.suffix in (".hpp", ".cpp") and "3rd" not in p.parts
    )
    for path in files:
        rel = path.relative_to(ROOT).as_posix()
        src_layer = layer_of(rel[len("bklv/") :])
        if src_layer == ".":
            continue  # 伞头 bklv.hpp 是给使用者聚合用的，不参与分层规则
        try:
            text = path.read_text(encoding="utf-8")
        except OSError:
            continue
        for m in INCLUDE_RE.finditer(text):
            target = m.group(1)[len("bklv/") :]
            if target.startswith("3rd/"):
                continue
            dst_layer = layer_of(target)
            if dst_layer == ".":
                continue
            counts[(src_layer, dst_layer)] = counts.get((src_layer, dst_layer), 0) + 1
            if (src_layer, dst_layer) in FORBIDDEN:
                edges.setdefault((src_layer, dst_layer, rel), []).append(target)
    return edges, counts, len(files)


def check_umbrella():
    """伞头可达范围：bklv/bklv.hpp 顺着 `#include "bklv/..."` 能走到的头，
    不许落在 backends/ 或 platforms/ 里，也不许出现 Vulkan 头。

    返回 [(可达文件, 原因)]，空表示通过。第三方（bklv/3rd/）是叶子，不再往下走。
    """
    problems = []
    seen = {UMBRELLA.resolve()}
    queue = [UMBRELLA]

    while queue:
        path = queue.pop()
        try:
            text = path.read_text(encoding="utf-8")
        except OSError:
            continue

        for header in UMBRELLA_FORBIDDEN_HEADERS:
            if f"#include <{header}>" in text or f'#include "{header}"' in text:
                problems.append((path.relative_to(ROOT).as_posix(), f"include 了 {header}"))

        for match in INCLUDE_RE.finditer(text):
            target = LIB / match.group(1)[len("bklv/"):]
            if not target.exists():
                continue
            rel = target.relative_to(LIB).as_posix()
            if rel.startswith("3rd/"):
                continue
            if rel.split("/")[0] in UMBRELLA_FORBIDDEN_DIRS:
                problems.append(
                    (path.relative_to(ROOT).as_posix(), f"可达 {target.relative_to(ROOT).as_posix()}")
                )
                continue
            resolved = target.resolve()
            if resolved in seen:
                continue
            seen.add(resolved)
            queue.append(target)

    return problems


def main():
    argv = sys.argv[1:]
    edges, counts, file_count = collect()
    umbrella = check_umbrella()

    if "--list" in argv:
        print(f"跨层依赖（扫描 {file_count} 个文件；同层 include 不计；★ = 被禁止的方向）")
        for (src, dst), n in sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])):
            if src == dst:
                continue
            mark = "★" if (src, dst) in FORBIDDEN else " "
            print(f"  {mark} {src:>9} → {dst:<9} {n:3d} 处")
        print("\n被禁止方向的明细：")
        for (src, dst, rel), targets in sorted(edges.items()):
            tag = "白名单" if (src, dst, rel) in BASELINE else "新增！"
            print(f"  [{tag}] {src} → {dst}  {rel}")
            for t in sorted(set(targets)):
                print(f"           bklv/{t}")
        print("\n伞头可达范围：bklv/bklv.hpp")
        if umbrella:
            for rel, why in umbrella:
                print(f"  ✗ {rel}：{why}")
        else:
            print("  ✓ 没有可达的 backends/ platforms/ 或 Vulkan 头")
        return 0

    current = set(edges.keys())
    added = sorted(current - BASELINE)
    stale = sorted(BASELINE - current)

    failed = False

    if added or stale:
        failed = True
        if added:
            print(f"分层依赖检查：新增 {len(added)} 条反向依赖")
            for src, dst, rel in added:
                print(f"  ✗ {rel}：{src} → {dst}（禁止 {src} 反向依赖 {dst}）")
                for t in sorted(set(edges[(src, dst, rel)])):
                    print(f"          bklv/{t}")
            print("  改法：把依赖挪到上层（由上层注入/注册），不要让底层认识上层；")
            print("        确实必须保留就在 docs/architecture-review.html 里记账并加进 BASELINE。")
        if stale:
            print(f"分层依赖检查：白名单有 {len(stale)} 条已过期（依赖已经消解）")
            for src, dst, rel in stale:
                print(f"  ✗ {rel}：{src} → {dst} 已不存在，请从 BASELINE 删掉这一条")

    if umbrella:
        failed = True
        print(f"伞头可达范围检查：bklv/bklv.hpp 碰到了不该碰的头（{len(umbrella)} 处）")
        for rel, why in umbrella:
            print(f"  ✗ {rel}：{why}")
        print("  改法：让 core 只认抽象接口，具体实现由上层（平台 / 后端）注入；")
        print("        图形 API 头（vulkan.h 这类）不许出现在伞头可达的头文件里。")

    if failed:
        return 1

    print(f"分层依赖检查：{len(current)} 条存量反向依赖（白名单内），无新增；"
          f"伞头可达范围内没有 backends/ platforms/ 或 Vulkan 头")
    return 0


if __name__ == "__main__":
    sys.exit(main())
