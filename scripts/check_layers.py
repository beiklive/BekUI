#!/usr/bin/env python3
"""分层依赖方向检查（约定见 AGENTS.md「分层」与 docs/architecture-review.html §2.1）。

    python3 scripts/check_layers.py          # 有新增反向依赖时退出码 1
    python3 scripts/check_layers.py --list   # 打印当前全部跨层依赖（含计数），总是 0

**为什么需要它**：bklv 的所有模块编进同一个 OBJECT 库、共用一个 include 根，所以"分层"
只有目录约定 —— 编译器不会因为 `core/resource.cpp` 反向 include 了 `widgets/` 而报错。
命名规范有 check_naming.py 兜着，"依赖方向"这一条只能靠评审维持；这个脚本把它变成门禁：
**禁止新增反向依赖**，存量（下面 BASELINE）先记账，谁消解谁删行。

## 现在的模块与方向

`bklv/` 下的第一级目录就是一层（玻璃子系统在 `render/glass/`，算 render）：

    底层基础设施   core / geom / json
    能力层         anim / media / render / paint / input
    领域层         theme（外观数据与解析）/ layer（层绘制实现）
    视图层         view（View / Box / 视图树 / JSON 视图解析）
    应用与适配层   app（App / Context）/ platform（SDL3、Switch 宿主适配）
    控件层         widgets（外观驱动控件、基础件、容器、列表、调试覆盖层）

一个模块可以往下依赖（widgets → view → theme → core），不能往上。上面这张表是"意图"，
真正被拦的是下面 FORBIDDEN 里列出的方向 —— 它们不是全部的上行组合，因为有些上行依赖
是**接口与服务**：core 里的 `platform.hpp` 声明 `RenderBackend*`、`services.hpp` 汇总
宿主可替换的音频/输入服务，这类"底层持有上层接口"的地方是刻意的（依赖倒置），不拦。

允许的方向（不检查，属正常分层）：

    widgets → view / theme / layer / anim / media / render / paint / input / core / json
    view    → theme / layer / anim / media / render / paint / input / core / json
    theme   → media / paint / json / geom / core
    layer   → theme / media / render / paint / json / geom / core
    app     → 上面所有（组合根；它会创建通知层、调试层，所以也用 widgets）
    platform→ render / input / media / core（平台创建并持有后端）
    core    → platform（工厂：方向由"编译期裁剪 + 工厂"反转，属预期）
    core    → render / media / anim / input（底层只持有它们的**接口**，见上）
    任何一层 → 3rd（第三方是叶子）

注意"层"是 `bklv/` 下的第一级目录：玻璃子系统在 `render/glass/`，算 render 而不是独立一层。

另外还查一条伞头可达范围（见 `check_umbrella()`）：使用者的入口 `bklv/bklv.hpp`
不许间接碰到具体后端/平台目录，也不许出现 Vulkan 头 —— 曾经
`bklv.hpp → shell.hpp → glass_service.hpp → vk_glass.hpp → <vulkan/vulkan.h>` 让每个
包含伞头的翻译单元都被拖进 Vulkan 头（评审 §4.2 P1）。
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIB = ROOT / "bklv"

# 被禁止的反向依赖：(源层, 目标层)。
#
# 只列"确实不该发生"的方向（见模块说明里的分层表）；刻意保留的依赖倒置
# （core → render/media/anim/input/platform）不在里面。
FORBIDDEN = {
    # 基础机制不许认识外观、视图树与控件
    ("core", "theme"),
    ("core", "layer"),
    ("core", "view"),
    ("core", "app"),
    ("core", "widgets"),
    # 几何是纯数学，不许认识绘制/外观/视图
    ("geom", "paint"),
    ("geom", "anim"),
    ("geom", "theme"),
    ("geom", "layer"),
    ("geom", "view"),
    ("geom", "app"),
    ("geom", "widgets"),
    # JSON 工具是通用能力，不许认识主题/视图
    ("json", "theme"),
    ("json", "layer"),
    ("json", "view"),
    ("json", "app"),
    ("json", "widgets"),
    # 媒体解码/字体/音频是能力，不许认识外观与控件
    ("media", "theme"),
    ("media", "layer"),
    ("media", "view"),
    ("media", "app"),
    ("media", "widgets"),
    # 绘制效果（渐变/阴影/焦点环）不许认识外观层与控件
    ("paint", "layer"),
    ("paint", "view"),
    ("paint", "app"),
    ("paint", "widgets"),
    # 渲染接口与后端不许认识外观/视图/控件
    ("render", "theme"),
    ("render", "layer"),
    ("render", "view"),
    ("render", "app"),
    ("render", "widgets"),
    # 动画机制可以作用于 View/Transition，但不许认识控件与 App
    ("anim", "layer"),
    ("anim", "app"),
    ("anim", "widgets"),
    # 输入分发可以驱动 View，但不许认识控件与 App
    ("input", "layer"),
    ("input", "app"),
    ("input", "widgets"),
    # 外观数据与解析不许认识视图树与控件（应用外观的那两处见 BASELINE）
    ("theme", "view"),
    ("theme", "app"),
    ("theme", "widgets"),
    # 层绘制实现只认上下文，不许认识视图树与控件
    ("layer", "view"),
    ("layer", "app"),
    ("layer", "widgets"),
    # 视图层不许认识组合根与具体控件
    ("view", "app"),
    ("view", "widgets"),
    # 适配层不许认识外观/视图/控件
    ("platform", "theme"),
    ("platform", "layer"),
    ("platform", "view"),
    ("platform", "widgets"),
}

# 存量白名单：(源层, 目标层, 相对仓库路径)。
# 这些是已经存在、且**评审时明确记录过**的反向依赖；消解之后要把对应行删掉，
# 留着不删这个脚本会报"白名单已过期"（和 CMake 源清单漏项检查同一个取向：
# 记账必须精确，否则白名单会慢慢变成摆设）。
BASELINE = {
    # 外观解析的结果要写回 View/Box（applyAppearanceBox / applyWidgetBox、属性表 setter）。
    # 拆开是"theme 只解析、view 负责应用"，但 apply* 与解析共用大量内部助手，
    # 硬拆会在两个模块间开一堆半成品接口，先记账。
    ("theme", "view", "bklv/theme/appearance.cpp"),
    ("theme", "view", "bklv/theme/attributes.cpp"),
    # 音效表（sound.json）是主题的一部分，SoundBank 读主题目录才知道在哪。
    ("media", "theme", "bklv/media/sound_bank.cpp"),
    # JSON 的错误信息要拼主题键路径（"color.button.defaultBg"），共用同一套命名规则。
    ("json", "theme", "bklv/json/json_util.hpp"),
}

# 伞头（使用者入口）不许间接碰到的目录前缀 / 头文件
UMBRELLA = ROOT / "bklv" / "bklv.hpp"
UMBRELLA_FORBIDDEN_PREFIXES = (
    "render/vk/",
    "render/gl/",
    "render/gles3/",
    "render/libnx/",
    "render/glass/vk/",
    "render/glass/gl/",
    "platform/",
)
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
        src_layer = layer_of(rel[len("bklv/"):])
        if src_layer == ".":
            continue  # 伞头 bklv.hpp 是给使用者聚合用的，不参与分层规则
        try:
            text = path.read_text(encoding="utf-8")
        except OSError:
            continue
        for m in INCLUDE_RE.finditer(text):
            target = m.group(1)[len("bklv/"):]
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
    不许落在具体后端 / 平台目录里，也不许出现 Vulkan 头。

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
            if rel.startswith(UMBRELLA_FORBIDDEN_PREFIXES):
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
            print("  ✓ 没有可达的具体后端 / 平台目录或 Vulkan 头")
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
          f"伞头可达范围内没有具体后端 / 平台目录或 Vulkan 头")
    return 0


if __name__ == "__main__":
    sys.exit(main())
