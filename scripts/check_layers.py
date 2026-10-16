#!/usr/bin/env python3
"""分层依赖方向检查（约定见 AGENTS.md「分层」与 docs/architecture-review.html §2.1）。

    python3 scripts/check_layers.py          # 有新增反向依赖时退出码 1
    python3 scripts/check_layers.py --list   # 打印当前全部跨层依赖（含计数），总是 0

**为什么需要它**：bklv 的 core / views / widgets / compile / glass / backends / platforms
全部编进同一个 OBJECT 库、共用一个 include 根，所以"分层"目前只是目录约定 ——
编译器不会因为 `core/app.cpp` 反向 include 了 `widgets/` 而报错。
命名规范有 check_naming.py 兜着，"依赖方向"这一条一直没有对应的检查，于是它只能靠评审维持。
这个脚本把约定变成门禁：**禁止新增反向依赖**，存量（下面 BASELINE）先记账，谁消解谁删行。

禁止的方向（左侧的层不许 include 右侧的层）：

    core    → widgets / views / compile / glass / backends
    compile → views / widgets
    views   → widgets
    glass   → backends

允许的方向（不在检查范围，属正常分层）：

    widgets → views / core / compile / glass    视图与外观驱动的控件在下游
    views → core                                语义控件只认 ImDrawList 与 core 接口
    compile → core                              解析实现用到 core 的数据类型
    glass → core                                玻璃服务用 logger 等
    backends → core / glass                     后端实现 core 的接口、glass 的层类型
    platforms → core / backends                 平台创建并持有后端
    core → platforms                            工厂：方向由"编译期裁剪 + 工厂"反转，属预期
    任何一层 → 3rd                              第三方是叶子
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
    ("core", "glass"),
    ("core", "backends"),
    ("compile", "views"),
    ("compile", "widgets"),
    ("views", "widgets"),
    ("glass", "backends"),
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
    # 玻璃服务层按值持有两个后端实现（应抽成玻璃侧的纯虚接口，见评审 §4.2 P1）。
    # 玻璃现在在 core/glass/ 下，所以这条是 core → backends（原来记作 glass → backends）。
    ("core", "backends", "bklv/core/glass/glass_service.hpp"),
    ("core", "backends", "bklv/core/glass/glass_service.cpp"),
}

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


def main():
    argv = sys.argv[1:]
    edges, counts, file_count = collect()

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
        return 0

    current = set(edges.keys())
    added = sorted(current - BASELINE)
    stale = sorted(BASELINE - current)

    if added or stale:
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
        return 1

    print(f"分层依赖检查：{len(current)} 条存量反向依赖（白名单内），无新增")
    return 0


if __name__ == "__main__":
    sys.exit(main())
