#!/usr/bin/env python3
"""检查主题资源的一致性（不联网、不依赖第三方库）。

校验三件事，任何一条不满足就说明资源树已经和代码/清单脱节：

1. `theme.json` 的 `files[]` 里每一项都真的有对应的 `.json`；
2. 目录里每个外观 `.json` 都被 `files[]` 登记过（否则它在 romfs 上永远不会被读）；
3. 外观里的 `import` 目标、`src` 图片都真的存在（`import` 允许回退 default 主题）。

用法：
    python3 scripts/check_theme.py [主题根目录，默认 resources/theme]
"""

import json
import sys
from pathlib import Path

DEFAULT_THEME = "default"


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def walk_strings(node, key):
    """递归收集所有 dict 里名为 key 的字符串值、以及列表里出现的目标名。"""
    found = []
    if isinstance(node, dict):
        for k, v in node.items():
            if k == key and isinstance(v, str):
                found.append(v)
            elif k == key and isinstance(v, list):
                found.extend(x for x in v if isinstance(x, str))
            else:
                found.extend(walk_strings(v, key))
    elif isinstance(node, list):
        for item in node:
            found.extend(walk_strings(item, key))
    return found


def collect_appearances(theme_dir: Path):
    """名字 = 相对主题根的路径去掉 .json，和 C++ 侧 theme_data.hpp 的约定一致。

    `theme.json` / `themes.json` 是元数据，`sound.json` 由 theme.json 的 `sound`
    键指名（不是外观，也不进 files[]），三样都不算外观。
    """
    names = {}
    for path in sorted(theme_dir.rglob("*.json")):
        rel = path.relative_to(theme_dir).as_posix()
        if rel in ("theme.json", "themes.json", "sound.json"):
            continue
        if rel.startswith("tokens/"):
            continue
        names[rel[: -len(".json")]] = path
    return names


def check_theme(root: Path, theme_dir: Path, default_apps, default_dir: Path, errors):
    theme_json = theme_dir / "theme.json"
    if not theme_json.is_file():
        errors.append(f"{theme_dir.name}: 缺少 theme.json")
        return

    meta = load_json(theme_json)
    listed = meta.get("files", [])
    apps = collect_appearances(theme_dir)

    listed_names = set()
    for entry in listed:
        if not entry.endswith(".json"):
            errors.append(f"{theme_dir.name}: files[] 里的 {entry!r} 不是 .json 路径")
            continue
        name = entry[: -len(".json")]
        listed_names.add(name)
        path = theme_dir / entry
        if not path.is_file():
            errors.append(f"{theme_dir.name}: files[] 登记了 {entry}，但文件不存在")
            continue
        try:
            doc = load_json(path)
        except json.JSONDecodeError as exc:
            errors.append(f"{theme_dir.name}/{entry}: JSON 语法错误（{exc}）")
            continue
        for target in walk_strings(doc, "import"):
            if target in listed_names or target in apps:
                continue
            if theme_dir.name != DEFAULT_THEME and target in default_apps:
                continue
            errors.append(f"{theme_dir.name}/{entry}: import {target!r} 找不到")

    for name, path in apps.items():
        if name not in listed_names:
            errors.append(f"{theme_dir.name}: {path.relative_to(theme_dir).as_posix()} 没登记在 files[] 里")

    # 图片等素材：src 相对主题根，允许回退 default
    for name in listed_names:
        if name not in apps:
            continue
        doc = load_json(apps[name])
        for src in walk_strings(doc, "src"):
            if (theme_dir / src).is_file():
                continue
            if theme_dir.name != DEFAULT_THEME and (default_dir / src).is_file():
                continue
            errors.append(f"{theme_dir.name}/{name}.json: src {src!r} 找不到")


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "resources/theme")
    if not root.is_dir():
        print(f"[失败] 主题目录不存在：{root}")
        return 1

    default_dir = root / DEFAULT_THEME
    default_apps = collect_appearances(default_dir) if default_dir.is_dir() else {}

    themes = sorted(p.parent for p in root.glob("*/theme.json"))
    if not themes:
        print(f"[失败] {root} 下没有任何主题")
        return 1

    errors = []
    for theme_dir in themes:
        check_theme(root, theme_dir, default_apps, default_dir, errors)

    if errors:
        print(f"[失败] {len(errors)} 处不一致：")
        for line in errors:
            print("  - " + line)
        return 1

    total = sum(len(collect_appearances(d)) for d in themes)
    print(f"[通过] {len(themes)} 个主题、{total} 份外观，清单与磁盘一致")
    return 0


if __name__ == "__main__":
    sys.exit(main())
