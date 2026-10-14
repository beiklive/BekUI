#!/usr/bin/env python3
"""接口命名与调用风格检查（约定见 AGENTS.md「接口命名与调用风格」）。

    python3 scripts/check_naming.py        # 有违例时退出码 1

只扫库自己的头文件（bklv/**，不含 3rd/），规则：
  1. 枚举值必须是 UPPER_SNAKE
  2. 类型（class/struct）必须是 PascalCase
  3. 公开方法里的状态查询（返回 bool）必须是 is*/has*/can*
     —— 例外：动作返回是否成功（init*/open/push*/pop*/step/recognize/fill/setTheme 等）
        与数据取值（get*）不算状态查询
  4. 布尔开关不得写成 enable*/disable*/show*/hide*（要 set<名词>Enabled/Visible/...）
  5. 文件名必须是 snake_case
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIB = ROOT / "bklv"

UPPER_SNAKE = re.compile(r"^[A-Z][A-Z0-9_]*$")
PASCAL = re.compile(r"^[A-Z][A-Za-z0-9]*$")
SNAKE_FILE = re.compile(r"^[a-z0-9_]+\.(hpp|cpp)$")

# 返回 bool 但属于"动作是否成功"的动词前缀（AGENTS.md 里写明允许）
ACTION_PREFIXES = (
    "init", "open", "close", "push", "pop", "step", "recognize", "fill",
    "set", "play", "trigger", "submit", "handle", "refresh", "load", "apply",
    "run", "dispatch", "insert", "backspace", "delete", "move", "update",
    "create", "mainLoop", "collect", "check", "ensure", "wants",
    # 动作类：读/写/请求/捕获/存取都算动作返回是否成功（约定见 AGENTS.md）
    "read", "write", "request", "capture", "save", "parse", "copy", "install",
    # prepare 也是动作（"准备好某样东西"，成败用返回值表示）：玻璃后端每帧准备背景用它
    "prepare",
)
# 布尔开关的非法动词前缀
BAD_BOOL_SETTER = re.compile(r"\bvoid\s+(enable|disable|show|hide)([A-Z]\w*)\s*\([^)]*\bbool\b")

problems = []


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def check_enums(path, text):
    for m in re.finditer(r"enum class (\w+)(?:\s*:\s*\w+)?\s*\{(.*?)\}", text, re.S):
        enum_name = m.group(1)
        if not PASCAL.match(enum_name):
            problems.append(f"{path}: 枚举类型名不是 PascalCase：{enum_name}")
        for raw in m.group(2).split(","):
            value = raw.split("=")[0].strip()
            if not value:
                continue
            if not UPPER_SNAKE.match(value):
                problems.append(
                    f"{path}: {enum_name}::{value} 不是 UPPER_SNAKE（约定：Visibility::GONE）")


def check_types(path, text):
    for m in re.finditer(r"\b(?:class|struct)\s+(\w+)", text):
        name = m.group(1)
        if not PASCAL.match(name):
            problems.append(f"{path}: 类型名不是 PascalCase：{name}")


def check_methods(path, text):
    for m in re.finditer(r"\bvoid\s+(enable|disable|show|hide)[A-Z]\w*\s*\([^)]*\bbool\b", text):
        problems.append(
            f"{path}: {m.group(1)}…(bool) 属于动词式布尔开关，"
            f"应写成 set<名词>Enabled/Visible（约定见 AGENTS.md）")

    # public 段里的 bool 查询
    for block in re.finditer(r"public:(.*?)(?:private:|protected:|\n\};)", text, re.S):
        for m in re.finditer(r"\bbool\s+(\w+)\s*\(", block.group(1)):
            name = m.group(1)
            if name.startswith(("is", "has", "can", "get")):
                continue
            if name.startswith(ACTION_PREFIXES):
                continue
            problems.append(
                f"{path}: bool {name}() 既不像状态查询（is*/has*/can*）"
                f"也不像动作（{', '.join(ACTION_PREFIXES[:4])}…）")


def main():
    files = sorted(p for p in LIB.rglob("*") if p.suffix in (".hpp", ".cpp") and "3rd" not in p.parts)
    for path in files:
        rel = path.relative_to(ROOT)
        if not SNAKE_FILE.match(path.name):
            problems.append(f"{rel}: 文件名不是 snake_case")
        if path.suffix != ".hpp":
            continue
        text = strip_comments(path.read_text(encoding="utf-8"))
        check_enums(rel, text)
        check_types(rel, text)
        check_methods(rel, text)

    if problems:
        print(f"命名检查：{len(problems)} 处不符合约定")
        for p in problems:
            print("  ✗", p)
        return 1

    print(f"命名检查：{len(files)} 个文件全部符合约定")
    return 0


if __name__ == "__main__":
    sys.exit(main())
