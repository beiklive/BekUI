#!/usr/bin/env python3
"""生成 docs/uml.html —— bklv（BekUI）的 UML 图（类图 / 包图 / 时序图）。

为什么自己画 SVG 而不是用 mermaid：
  这份页面要能**离线打开**（晚上在自己机器上直接双击看），mermaid 走 CDN 一旦没网就是空白。
  所以图全部由本脚本生成内联 SVG，页面零外部依赖。

用法：python3 scripts/gen_uml.py        # 输出 docs/uml.html
"""

import html
import os

# ---------------------------------------------------------------- SVG 原语

# 配色（浅色主题；页面里另有一份深色主题的 CSS 覆盖）
INK = "#1f2430"
MUTED = "#5b6472"
LINE = "#9aa4b2"
BOX_FILL = "#ffffff"
HEAD_FILL = "#eef2f7"
ACCENT = "#0e639c"
ACCENT_FILL = "#e3f0fb"
WARN = "#b35c00"
WARN_FILL = "#fdf1e0"
OK = "#2f7d32"
OK_FILL = "#e6f4e6"


class Diagram:
    """一张图：收集盒子与连线，最后吐一段 <svg>。"""

    def __init__(self, title, width, height):
        self.title = title
        self.width = width
        self.height = height
        self.boxes = {}      # id -> (x, y, w, h)
        self.parts = []      # svg 片段
        self.notes = 0

    # ---- 盒子 ----
    def cls(self, cid, name, rows=(), x=0, y=0, w=240, kind="class", stereotype=None,
            badge=None):
        row_h = 20
        head_h = 34 if stereotype else 26
        h = head_h + row_h * len(rows) + 8
        self.boxes[cid] = (x, y, w, h)
        fill, head = BOX_FILL, HEAD_FILL
        if kind == "abstract":
            head = "#e9e6f7"
        elif kind == "iface":
            head = "#e6f0e6"
        elif kind == "theme":
            head, fill = "#fdeeee", "#fff8f8"
        elif kind == "group":
            head, fill = "#eaeef4", "#f7f9fc"
        s = [f'<g class="node">']
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="7" '
                 f'fill="{fill}" stroke="{LINE}" stroke-width="1.2"/>')
        s.append(f'<path d="M{x} {y+head_h} h{w}" stroke="{LINE}" stroke-width="1"/>')
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{head_h}" rx="7" fill="{head}"/>')
        s.append(f'<rect x="{x}" y="{y+head_h-7}" width="{w}" height="7" fill="{head}"/>')
        if stereotype:
            s.append(f'<text x="{x+w/2}" y="{y+15}" class="stereo">{html.escape(stereotype)}</text>')
        weight = "700" if kind in ("class", "abstract", "iface", "theme") else "600"
        s.append(f'<text x="{x+w/2}" y="{y + (28 if stereotype else 19)}" class="cname" '
                 f'font-weight="{weight}">{html.escape(name)}</text>')
        for i, row in enumerate(rows):
            ry = y + head_h + 14 + i * row_h
            cls_attr = "row-sep" if row.strip().startswith("--") else "row"
            txt = row[2:].strip() if row.strip().startswith("--") else row
            if row.strip().startswith("--"):
                s.append(f'<path d="M{x+6} {ry-14} h{w-12}" stroke="#dbe1e8" stroke-width="1"/>')
                s.append(f'<text x="{x+10}" y="{ry}" class="row dim">{html.escape(txt)}</text>')
            else:
                s.append(f'<text x="{x+10}" y="{ry}" class="row">{html.escape(row)}</text>')
        if badge:
            bw = 8 * len(badge) + 14
            s.append(f'<rect x="{x+w-bw-6}" y="{y+4}" width="{bw}" height="16" rx="8" '
                     f'fill="{ACCENT_FILL}" stroke="{ACCENT}" stroke-width="0.8"/>')
            s.append(f'<text x="{x+w-bw/2-6}" y="{y+16}" class="badge">{html.escape(badge)}</text>')
        s.append('</g>')
        self.parts.append("".join(s))
        return cid

    def group(self, cid, title, x, y, w, h, note=None):
        self.boxes[cid] = (x, y, w, h)
        s = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="10" fill="#f2f5f9" '
             f'stroke="#c9d3df" stroke-width="1.2" stroke-dasharray="6 4"/>',
             f'<text x="{x+12}" y="{y+20}" class="groupname">{html.escape(title)}</text>']
        if note:
            s.append(f'<text x="{x+12}" y="{y+h-10}" class="dim">{html.escape(note)}</text>')
        self.parts.append("".join(s))
        return cid

    def note(self, text, x, y, w=None, lines=1):
        w = w or (7 * max(len(t) for t in text.split("\n")) + 24)
        h = 18 * lines + 14
        self.parts.append(
            f'<g><rect x="{x}" y="{y}" width="{w}" height="{h}" rx="5" fill="{WARN_FILL}" '
            f'stroke="{WARN}" stroke-width="1"/>'
            + "".join(f'<text x="{x+10}" y="{y+20+i*17}" class="note">{html.escape(t)}</text>'
                      for i, t in enumerate(text.split("\n")))
            + '</g>')
        return (x, y, w, h)

    # ---- 连线 ----
    def _anchor(self, cid, side):
        x, y, w, h = self.boxes[cid]
        return {"l": (x, y + h / 2), "r": (x + w, y + h / 2),
                "t": (x + w / 2, y), "b": (x + w / 2, y + h)}[side]

    def link(self, a, b, kind="assoc", label=None, a_side=None, b_side=None, route=None,
             label_dx=6, label_dy=-6, dashed=False):
        """kind: inherit / compose / assoc / depend"""
        ax, ay, aw, ah = self.boxes[a]
        bx, by, bw, bh = self.boxes[b]
        a_side = a_side or ("r" if bx > ax + aw * 0.5 else "l")
        b_side = b_side or ("l" if ax < bx else "r")
        p1 = self._anchor(a, a_side)
        p2 = self._anchor(b, b_side)
        pts = [p1]
        if route:
            pts += route
        elif a_side in ("l", "r") and b_side in ("l", "r"):
            midx = (p1[0] + p2[0]) / 2
            pts += [(midx, p1[1]), (midx, p2[1])]
        elif a_side in ("t", "b") and b_side in ("t", "b"):
            midy = (p1[1] + p2[1]) / 2
            pts += [(p1[0], midy), (p2[0], midy)]
        elif a_side in ("l", "r"):
            pts += [(p2[0], p1[1])]
        else:
            pts += [(p1[0], p2[1])]
        pts.append(p2)
        d = "M" + " L".join(f"{round(px,1)} {round(py,1)}" for px, py in pts)
        dash = ' stroke-dasharray="5 4"' if (dashed or kind == "depend") else ""
        self.parts.append(f'<path d="{d}" fill="none" stroke="{LINE}" stroke-width="1.4"{dash}/>')
        # 箭头
        (ex, ey), (px_, py_) = pts[-1], pts[-2]
        ang = 0.0
        import math
        ang = math.atan2(ey - py_, ex - px_)
        if kind == "inherit":
            size = 11
            bx1 = ex - size * math.cos(ang - 0.4); by1 = ey - size * math.sin(ang - 0.4)
            bx2 = ex - size * math.cos(ang + 0.4); by2 = ey - size * math.sin(ang + 0.4)
            self.parts.append(f'<path d="M{ex} {ey} L{round(bx1,1)} {round(by1,1)} '
                              f'L{round(bx2,1)} {round(by2,1)} Z" fill="#ffffff" stroke="{LINE}" '
                              f'stroke-width="1.4"/>')
        elif kind == "compose":
            size = 13
            mx = ex - size * math.cos(ang); my = ey - size * math.sin(ang)
            lx = mx - 7 * math.cos(ang - 1.2); ly = my - 7 * math.sin(ang - 1.2)
            rx = mx - 7 * math.cos(ang + 1.2); ry = my - 7 * math.sin(ang + 1.2)
            self.parts.append(f'<path d="M{ex} {ey} L{round(lx,1)} {round(ly,1)} '
                              f'L{round(mx,1)} {round(my,1)} L{round(rx,1)} {round(ry,1)} Z" '
                              f'fill="{INK}" stroke="{INK}" stroke-width="1"/>')
        else:
            size = 10
            bx1 = ex - size * math.cos(ang - 0.45); by1 = ey - size * math.sin(ang - 0.45)
            bx2 = ex - size * math.cos(ang + 0.45); by2 = ey - size * math.sin(ang + 0.45)
            self.parts.append(f'<path d="M{round(bx1,1)} {round(by1,1)} L{ex} {ey} '
                              f'L{round(bx2,1)} {round(by2,1)}" fill="none" stroke="{LINE}" '
                              f'stroke-width="1.4"/>')
        if label:
            mx = (p1[0] + p2[0]) / 2 + label_dx
            my = (p1[1] + p2[1]) / 2 + label_dy
            w = 6.6 * len(label) + 10
            self.parts.append(f'<rect x="{mx-w/2}" y="{my-12}" width="{w}" height="16" rx="4" '
                              f'fill="#ffffff" opacity="0.92"/>')
            self.parts.append(f'<text x="{mx}" y="{my}" class="edge">{html.escape(label)}</text>')

    def svg(self, caption=None):
        cap = f'<figcaption>{html.escape(caption)}</figcaption>' if caption else ""
        return (f'<figure>{cap}<svg viewBox="0 0 {self.width} {self.height}" '
                f'width="100%" xmlns="http://www.w3.org/2000/svg" '
                f'font-family="ui-sans-serif,-apple-system,Segoe UI,Helvetica,Arial">'
                + "".join(self.parts) + '</svg></figure>')


class Sequence:
    """时序图：竖生命线 + 横向消息。"""

    def __init__(self, width, actors, steps, note=None):
        self.width = width
        self.actors = actors              # [(id, 名字, 说明)]
        self.steps = steps                # [(from, to, 文本, kind)] kind: call/ret/self
        self.note = note
        self.lane = 190
        self.top = 64
        self.step_h = 34

    def svg(self, caption=None):
        n = len(self.actors)
        W = max(self.width, self.lane * n + 40)
        H = self.top + self.step_h * len(self.steps) + 90
        xs = {}
        for i, (aid, _, _) in enumerate(self.actors):
            xs[aid] = 30 + self.lane * i + self.lane / 2
        s = [f'<figure>{"<figcaption>" + html.escape(caption) + "</figcaption>" if caption else ""}',
             f'<svg viewBox="0 0 {W} {H}" width="100%" '
             f'xmlns="http://www.w3.org/2000/svg" '
             f'font-family="ui-sans-serif,-apple-system,Segoe UI,Helvetica,Arial">']
        for aid, name, desc in self.actors:
            x = xs[aid]
            s.append(f'<rect x="{x-80}" y="16" width="160" height="42" rx="7" fill="{HEAD_FILL}" '
                     f'stroke="{LINE}"/>')
            s.append(f'<text x="{x}" y="36" class="cname">{html.escape(name)}</text>')
            s.append(f'<text x="{x}" y="51" class="dim">{html.escape(desc)}</text>')
            s.append(f'<path d="M{x} 58 V{H-40}" stroke="#c9d3df" stroke-width="1.2" '
                     f'stroke-dasharray="6 6"/>')
        y = self.top + 16
        for i, (a, b, text, kind) in enumerate(self.steps):
            if kind == "self":
                x = xs[a]
                s.append(f'<path d="M{x} {y} h54 v18 h-54" fill="none" stroke="{ACCENT}" '
                         f'stroke-width="1.4"/>')
                s.append(f'<path d="M{x+54} {y+18} l-7 -4 v8 z" fill="{ACCENT}"/>')
                s.append(f'<text x="{x+62}" y="{y+8}" class="msg">{html.escape(text)}</text>')
            else:
                x1, x2 = xs[a], xs[b]
                dash = ' stroke-dasharray="5 4"' if kind == "ret" else ""
                s.append(f'<path d="M{x1} {y} H{x2}" stroke="{ACCENT if kind=="call" else MUTED}" '
                         f'stroke-width="1.5"{dash}/>')
                d = 1 if x2 > x1 else -1
                s.append(f'<path d="M{x2} {y} l{-9*d} -5 v10 z" '
                         f'fill="{ACCENT if kind=="call" else MUTED}"/>')
                tx = (x1 + x2) / 2
                w = 6.4 * len(text) + 12
                s.append(f'<rect x="{tx-w/2}" y="{y-19}" width="{w}" height="17" rx="4" '
                         f'fill="#ffffff" stroke="#e2e7ee"/>')
                s.append(f'<text x="{tx}" y="{y-6}" class="msg">{html.escape(text)}</text>')
            y += self.step_h
        if self.note:
            s.append(f'<text x="30" y="{H-18}" class="note">{html.escape(self.note)}</text>')
        s.append('</svg></figure>')
        return "".join(s)


# ---------------------------------------------------------------- 图 1：模块总览

def diagram_packages():
    d = Diagram("模块总览", 1520, 800)
    d.cls("host", "examples/（宿主示例）", ["embed_imgui_gl / multi_context / switch", "cmake_consumer：安装后 find_package"],
          x=60, y=30, w=360, kind="group")
    d.cls("switch", "Switch / 桌面", ["同一套上层代码，换平台实现"],
          x=740, y=30, w=240, kind="group")

    d.group("lib", "bklv（BekUI）—— 库本体，自包含", 60, 140, 1400, 470,
            note="上层与图形 API 无关：示例不写任何 GL / Vulkan 代码")

    d.cls("fnd", "foundation/（基础）", ["core/ 机制 / 资源 / 日志 / 服务",
                                        "geom/ 几何与变换", "json/ JSON 工具与策略",
                                        "anim/ 缓动与转场"],
          x=90, y=190, w=330, kind="group")
    d.cls("gfx", "graphics/（绘制）", ["paint/ 渐变 / 阴影 / 焦点框",
                                      "render/ RenderBackend 接口",
                                      "  gl / vk / gles3 / libnx", "  glass/{gl,vk}"],
          x=450, y=190, w=330, kind="group")
    d.cls("ui", "ui/（界面）", ["view/ View / Box（Yoga）/ ViewTemplate",
                               "theme/ ThemeData / Appearance / 图层表",
                               "layer/ 六个内置层 + 注册模板",
                               "input/ 键盘手柄手势 / 焦点 / IME",
                               "app/ App 帧循环 / Context / 通知层"],
          x=90, y=420, w=640, kind="group")
    d.cls("widgets", "ui/widgets/（控件）", ["appearance / controls / basic",
                                            "container / list / debug"],
          x=770, y=420, w=290, kind="group")
    d.cls("sys", "system/（支撑）", ["media/ 字体 / 图片 / 音频",
                                    "platform/ SDL3 / libnx"],
          x=1100, y=420, w=290, kind="group")

    d.group("third", "3rd/（全部走 git submodule，构建脚本自己编）", 60, 640, 1400, 140)
    for i, (name, use) in enumerate([
            ("imgui", "GUI 与绘制"), ("yoga", "Flexbox 布局"), ("SDL", "窗口 / 输入"),
            ("stb", "PNG/JPEG/GIF"), ("webp", "WebP 可选"), ("lunasvg", "SVG 可选"),
            ("json", "JSON / i18n"), ("tweeny", "补间动画")]):
        d.cls(f"t{i}", name, [use], x=90 + i * 168, y=690, w=152, kind="group")

    d.link("host", "ui", kind="depend", label="链接 bklv::bklv", b_side="t", a_side="b",
           route=[(240, 100), (240, 120), (410, 120), (410, 420)])
    d.link("switch", "sys", kind="depend", b_side="t", a_side="b",
           route=[(860, 100), (1245, 100), (1245, 420)])
    d.link("fnd", "t6", kind="depend", dashed=True, a_side="b", b_side="t",
           route=[(200, 420), (200, 690)])
    d.link("gfx", "t0", kind="depend", dashed=True, a_side="b", b_side="t",
           route=[(560, 420), (560, 660), (166, 660), (166, 690)])
    d.link("sys", "t2", kind="depend", dashed=True, a_side="b", b_side="t",
           route=[(1245, 640), (1245, 665), (502, 665), (502, 690)], label="SDL3")
    return d.svg("图 1：谁依赖谁 —— 库自包含，3rd 全部随构建一起编；上层不出现任何图形 API 代码")


# ---------------------------------------------------------------- 图 2：视图体系

def diagram_view_tree():
    d = Diagram("视图体系", 1740, 960)
    d.cls("view", "View", ["# id / parent / 布局盒（Yoga 节点）", "+ update()  + frame(ctx)  + draw()",
                           "+ invalidate()  + attributes()", "+ hitTest()  + triggerAction()",
                           "+ applyTheme()  + getClassString()"],
          x=690, y=30, w=360, kind="abstract", stereotype="«abstract»")
    d.cls("label", "Label", ["文本 / 换行 / 截断"], x=60, y=250, w=170)
    d.cls("image", "Image", ["纹理 / 动图换帧"], x=250, y=250, w=170)
    d.cls("padding", "Padding", ["占位挤压 / grow=1"], x=440, y=250, w=170)
    d.cls("box", "Box", ["子视图容器", "+ addView / removeView", "+ getView / asBox",
                         "+ getDefaultFocus"], x=690, y=250, w=360, kind="class")
    d.cls("debug", "DebugLogView / DebugLayer", ["直接画 ImDrawList，不建视图"],
          x=1420, y=250, w=260, kind="group")

    d.cls("av", "AppearanceView", ["外观名 + AppearanceInstance", "+ setAppearanceName / Variant",
                                   "+ drawCustomLayer（语义绘制）", "+ getLayerText / Image / Font",
                                   "+ setIcon / setInfoText"], x=60, y=470, w=340, kind="class")
    d.cls("scroll", "ScrollingFrame", ["flex-basis:0 + shrink:1", "滚动手势 / 惯性"],
          x=440, y=470, w=250)
    d.cls("hscroll", "HScrollingFrame", ["横向滚动"], x=730, y=470, w=190)
    d.cls("shell", "Shell", ["Header / Footer / DebugLayer", "内容栈 + 转场"],
          x=960, y=470, w=250)
    d.cls("dialog", "Dialog", ["遮罩 + 面板 + 焦点锁"], x=1250, y=470, w=210)
    d.cls("recycler", "RecyclerFrame", ["列表回收（单段 / 分段）", "+ layoutCell"], x=1500, y=470, w=210)

    d.group("avsub", "AppearanceView 的派生：主题里 54 份外观（base/* + widget/*）", 60, 680, 1620, 240,
            note="装饰层走 JSON（layers），语义绘制留在 C++（drawCustomLayer）")
    sub = [
        ("Button", ["8 个变体：text / icon / icon_text", "dropdown / switch_* / info", "→ SelectorButton（选择器）"]),
        ("Cell 家族", ["BooleanCell（文字 开/关 + 弹字）", "RadioCell / SliderCell",
                       "SelectorCell（弹 Dropdown）", "DetailCell / InputCell / TabCell"]),
        ("布局件", ["Header / BottomBar / HintsBar", "StatusArea / Sidebar / TabFrame",
                    "Pager / AppletFrame"]),
        ("弹层", ["DialogPanel / DialogBackdrop", "DropdownPanel / DropdownOption",
                  "Notification"]),
        ("装饰件", ["BatteryIcon / WirelessIcon", "AccountWidget / HeaderIcon",
                    "HintItemView（按键提示）", "Slider（滑块轨道自绘）"]),
    ]
    for i, (name, rows) in enumerate(sub):
        d.cls(f"sub{i}", name, rows, x=90 + i * 318, y=730, w=290, kind="group")

    d.link("view", "box", kind="inherit", b_side="t", a_side="b", route=[(870, 210), (870, 250)])
    d.link("view", "label", kind="inherit", b_side="t", a_side="l", route=[(690, 210), (145, 210), (145, 250)])
    d.link("view", "image", kind="inherit", b_side="t", a_side="l", route=[(690, 228), (335, 228), (335, 250)])
    d.link("view", "padding", kind="inherit", b_side="t", a_side="l", route=[(690, 246), (525, 246), (525, 250)])
    d.link("view", "debug", kind="inherit", b_side="t", a_side="r", route=[(1050, 195), (1550, 195), (1550, 250)])
    d.link("box", "av", kind="inherit", a_side="l", b_side="t", route=[(690, 330), (230, 330), (230, 470)])
    d.link("box", "scroll", kind="inherit", a_side="b", b_side="t", route=[(820, 400), (565, 400), (565, 470)])
    d.link("box", "hscroll", kind="inherit", a_side="b", b_side="t", route=[(690, 430), (825, 430), (825, 470)])
    d.link("box", "shell", kind="inherit", a_side="b", b_side="t", route=[(1050, 400), (1085, 400), (1085, 470)])
    d.link("box", "dialog", kind="inherit", a_side="r", b_side="t", route=[(1050, 300), (1355, 300), (1355, 470)])
    d.link("scroll", "recycler", kind="inherit", a_side="b", b_side="t",
           route=[(565, 530), (1605, 530), (1605, 470)], label="列表回收", label_dx=0, label_dy=26)
    d.link("av", "sub0", kind="compose", a_side="b", b_side="t", route=[(230, 610), (235, 730)])
    d.link("av", "sub1", kind="compose", a_side="b", b_side="t", route=[(150, 640), (553, 640), (553, 730)])
    d.link("av", "sub2", kind="compose", a_side="b", b_side="t", route=[(200, 660), (871, 660), (871, 730)])
    d.link("av", "sub3", kind="compose", a_side="b", b_side="t", route=[(250, 670), (1189, 670), (1189, 730)])
    d.link("av", "sub4", kind="compose", a_side="b", b_side="t", route=[(300, 690), (1507, 690), (1507, 730)])
    d.note("Box 是容器：AppearanceView / ScrollingFrame / Shell / Dialog 都继承它，\n"
           "只有 Box 的子树参与命中测试与焦点导航（hitTest 沿树走）", 1140, 30, lines=2)
    return d.svg("图 2：视图体系 —— 三层：View（绘制/生命周期）→ Box（容器）→ 具体控件；"
                 "控件分两族：AppearanceView（外观驱动）与纯 C++ 绘制（Label/Image）")


# ---------------------------------------------------------------- 图 3：主题与外观

def diagram_theme():
    d = Diagram("主题与外观", 1600, 780)
    d.cls("res", "Resource", ["resources/ 按可执行文件目录解析", "Switch 走 romfs:/"],
          x=60, y=40, w=250, kind="iface", stereotype="«namespace»")
    d.cls("themes", "themes.json", ["主题清单（不扫目录）"], x=350, y=40, w=200, kind="group")
    d.cls("td", "ThemeData", ["111 色 / 143 数值", "54 份控件外观",
                              "+ color(key)  + number(key)", "+ loadFromDirectory()"],
          x=590, y=40, w=300, kind="theme")
    d.cls("app", "Appearance", ["box（盒模型）", "layers（绘制结构）", "states（状态动效）"],
          x=940, y=40, w=220, kind="class")
    d.cls("ai", "AppearanceInstance", ["每控件的已解析值", "+ setState / getAppearance",
                                       "+ isAnimating / step()"], x=1200, y=40, w=280, kind="class")
    d.cls("av", "AppearanceView", ["持有 AppearanceInstance", "绘制只读已解析值"],
          x=1200, y=250, w=280, kind="class")
    d.cls("vt", "ViewTemplate", ["两段式：模板（可缓存）", "→ instantiate()（解析 @i18n/）"],
          x=590, y=300, w=300, kind="class")
    d.cls("json", "json_view / attributes", ["\"type\" + 属性 + children + include",
                                             "深度 32 / 节点 4096 / include 栈 32"],
          x=60, y=300, w=380, kind="group")
    d.cls("at", "AttributeTable", ["按类静态的属性表", "handler = 函数指针"],
          x=590, y=470, w=300, kind="class")
    d.cls("view", "View 树", ["构造后 applyTheme()", "换肤 = 重新取外观 + 重排"],
          x=1200, y=470, w=280, kind="class")
    d.cls("doc", "JSON 文档编辑（外部工具）", ["直接读写主题外观 JSON", "改完热重载就变预览"],
          x=60, y=560, w=380, kind="group")

    d.link("res", "themes", kind="assoc", label="读", a_side="r", b_side="l")
    d.link("themes", "td", kind="assoc", label="files[] 清单", a_side="r", b_side="l")
    d.link("td", "app", kind="assoc", label="解析 base/* widget/*", a_side="r", b_side="l",
           label_dy=-8)
    d.link("app", "ai", kind="compose", label="运行时实例", a_side="r", b_side="l")
    d.link("ai", "av", kind="assoc", a_side="b", b_side="t", route=[(1340, 210), (1340, 250)])
    d.link("json", "vt", kind="assoc", a_side="r", b_side="l", label="createFromJSONFile")
    d.link("vt", "at", kind="assoc", a_side="b", b_side="t", route=[(740, 420), (740, 470)])
    d.link("at", "view", kind="assoc", a_side="r", b_side="l", label="构造时读",
           route=[(890, 520), (1200, 520)])
    d.link("av", "view", kind="compose", a_side="b", b_side="t", route=[(1340, 400), (1340, 470)],
           label="UI 树")
    d.link("doc", "td", kind="depend", dashed=True, a_side="t", b_side="b",
           route=[(250, 470), (250, 380), (250, 190)])
    d.note("① 装饰层（底/边框/文字/图片/状态动效）走 JSON；语义绘制（换行、进度弧、滑块轨道）留 C++\n"
           "② draw 里不查主题：值在构造 / applyTheme() 时取定\n"
           "③ 显式 set 过的值主题不再覆盖（setXxxExplicit）", 60, 660, lines=3)
    return d.svg("图 3：主题 → 外观 → 实例 → 控件；右侧是换肤路径，左侧是 JSON 视图描述的装载路径")


# ---------------------------------------------------------------- 图 4：运行时装配

def diagram_runtime():
    d = Diagram("运行时装配", 1740, 560)
    d.cls("app", "App", ["+ init() / run() / frame()   + setRootView() / setTheme()",
                         "+ pushOverlay() / requestLayout()   + requestScreenshot()",
                         "onDrawWindows / onPrepareDrawData（通用钩子）"], x=60, y=30, w=620, kind="class")

    # 子系统收进一个分组框：App 持有它们，逐条画箭头只会把图弄乱（而且语义上就是"成员"）
    d.group("subs", "App 的成员子系统（回调都回主线程）", 60, 250, 620, 270)
    subs = [("input", "InputManager", ["焦点 / 导航 / 模态栈", "KeyRepeater 长按连发"]),
            ("evt", "EventBus", ["RAII 订阅（句柄析构退订）", "无 RTTI：静态 token 当 key"]),
            ("tasks", "task / time", ["task::delay + 队列", "pumpMainThread 每帧泵"]),
            ("ime", "ImeManager", ["平台接管（swkbd）", "或控件自绘软键盘"]),
            ("log", "Logger", ["环形缓冲 + 文件输出", "DebugLogView 读缓冲"]),
            ("audio", "AudioMixer", ["音效播放 / 音量"])]
    for i, (cid, name, rows) in enumerate(subs):
        col, row = i % 3, i // 3
        d.cls(cid, name, rows, x=80 + col * 200, y=290 + row * 110, w=185, kind="group")
    d.link("app", "subs", kind="compose", label="持有", a_side="b", b_side="t",
           route=[(370, 165), (370, 250)], label_dx=-46, label_dy=26)

    d.cls("plat", "Platform", ["+ initWindow / pollEvents", "+ createBackend()",
                               "+ openIme / gamepad"], x=760, y=30, w=380, kind="abstract",
          stereotype="«abstract»")
    d.cls("sdl", "PlatformSDL", ["桌面：SDL3 + 输入"], x=760, y=290, w=185)
    d.cls("psw", "PlatformSwitch", ["libnx：swkbd / pl 字体"], x=955, y=290, w=185)
    d.cls("rb", "RenderBackend", ["+ createTexture / updateTexture", "+ renderDrawData",
                                  "+ readPixels（GL 已实现）"], x=1200, y=30, w=480,
          kind="abstract", stereotype="«abstract»")
    d.cls("gl", "OpenGLBackend", ["GL 3.x core（运行时加载）"], x=1200, y=290, w=235)
    d.cls("vk", "VulkanBackend", ["动态渲染 + 交换链 UNORM", "+ VulkanUiPassHook 接缝"],
          x=1445, y=290, w=235)

    d.link("app", "plat", kind="compose", label="持有", a_side="r", b_side="l", label_dy=-8)
    d.link("app", "rb", kind="compose", label="持有", a_side="r", b_side="t",
           route=[(430, 200), (430, 230), (1500, 230), (1500, 30)], label_dx=120, label_dy=-12)
    d.link("plat", "sdl", kind="inherit", a_side="b", b_side="t", route=[(850, 160), (850, 290)])
    d.link("plat", "psw", kind="inherit", a_side="b", b_side="t", route=[(1050, 160), (1050, 290)])
    d.link("rb", "gl", kind="inherit", a_side="b", b_side="t", route=[(1320, 160), (1320, 290)])
    d.link("rb", "vk", kind="inherit", a_side="b", b_side="t", route=[(1560, 160), (1560, 290)])
    return d.svg("图 4：App 是装配点：由 Platform 创建窗口与后端；输入 / 事件 / 定时 / IME / 日志 / 音频都是它的成员")


def sequence_frame():
    s = Sequence(1200, [("app", "App", "主循环"),
                        ("plat", "Platform", "SDL / libnx"),
                        ("view", "View 树", "布局与绘制"),
                        ("back", "RenderBackend", "GL / Vulkan")],
                 [("plat", "app", "pollEvents()（键盘 / 手柄 / 指针）", "call"),
                  ("app", "view", "invalidate() → 冒泡到根标脏", "call"),
                  ("app", "view", "YGNodeCalculateLayout（一帧最多一次）", "call"),
                  ("app", "view", "notifyLayout() → update() → frame(ctx)", "call"),
                  ("view", "view", "draw()：画进 ImGui 绘制列表", "self"),
                  ("app", "view", "onDrawWindows()：宿主自己的 ImGui 窗口", "call"),
                  ("app", "app", "ImGui::Render() → 生成 ImDrawData", "self"),
                  ("app", "app", "onPrepareDrawData()（通用钩子）", "self"),
                  ("app", "back", "renderDrawData()（GL 或 Vulkan）", "call"),
                  ("back", "plat", "present()", "call")],
                 note="布局是延迟的：invalidate() 只标脏，真正的重排由 App::frame 统一做（静态页面 0 次/秒）")
    return s.svg("图 5：一帧的时序（onDrawWindows 留给需要自己 ImGui 面板的宿主；demo 的日志窗口已是视图）")


def sequence_theme():
    s = Sequence(1200, [("user", "用户", "菜单 / 代码"),
                        ("app", "App", "装配点"),
                        ("td", "ThemeData", "主题数据"),
                        ("view", "View 树", "控件"),
                        ("back", "RenderBackend", "纹理 / 绘制")],
                 [("user", "app", "setTheme(\"ios\")", "call"),
                  ("app", "td", "loadFromDirectory()：清单驱动，不扫目录", "call"),
                  ("td", "app", "111 色 / 143 数值 / 54 份外观", "ret"),
                  ("app", "view", "applyThemeRecursive()：重新取外观值", "call"),
                  ("view", "view", "清除旧补间 → 按 states 重算目标", "self"),
                  ("app", "view", "requestLayout()（缩放 / 尺寸可能变）", "call"),
                  ("app", "app", "下一帧：重排 + 重绘", "self"),
                  ("app", "back", "纹理失效的图片重新上传", "call")],
                 note="换肤不改 C++：整份主题就是 resources/theme/<名>/ 下的一组 JSON")
    return s.svg("图 6：换主题 / 热重载的时序（改完 JSON 走同一条路，界面立刻变）")


# ---------------------------------------------------------------- HTML 组装

def build_html(path_out):
    import subprocess
    try:
        rev = subprocess.run(["git", "rev-parse", "--short", "HEAD"], capture_output=True,
                             text=True, cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
                             ).stdout.strip() or "-"
    except Exception:
        rev = "-"

    sections = [
        ("packages", "1 · 模块总览", "谁依赖谁、库边界在哪", diagram_packages()),
        ("views", "2 · 视图体系", "View → Box → 控件；两族控件的分工", diagram_view_tree()),
        ("theme", "3 · 主题与外观", "主题 JSON → Appearance → 实例 → 控件", diagram_theme()),
        ("runtime", "4 · 运行时装配", "App / Platform / RenderBackend 的关系", diagram_runtime()),
        ("frame", "5 · 一帧时序（图 5）", "从 pollEvents 到 present", sequence_frame()),
        ("switch", "6 · 换主题时序（图 6）", "换肤 / 热重载", sequence_theme()),
    ]

    nav = "".join(f'<a href="#{sid}">{html.escape(title)}</a>' for sid, title, _, _ in sections)
    body = []
    for sid, title, sub, svg in sections:
        body.append(f'<section id="{sid}"><h2>{html.escape(title)}</h2>'
                    f'<p class="lead">{html.escape(sub)}</p>{svg}</section>')

    legend = """
    <div class="legend">
      <span><svg width="46" height="14"><path d="M2 10 H34" stroke="#9aa4b2" stroke-width="1.4"/>
        <path d="M44 10 L34 5 L34 15 Z" fill="#fff" stroke="#9aa4b2" stroke-width="1.4"/></svg> 继承（空心三角）</span>
      <span><svg width="46" height="14"><path d="M2 10 H34" stroke="#9aa4b2" stroke-width="1.4"/>
        <path d="M2 10 L8 6 L14 10 L8 14 Z" fill="#1f2430"/></svg> 组合（实心菱形，生命周期一致）</span>
      <span><svg width="46" height="14"><path d="M2 10 H40" stroke="#9aa4b2" stroke-width="1.4"/>
        <path d="M44 10 L34 5 L34 15" fill="none" stroke="#9aa4b2" stroke-width="1.4"/></svg> 关联 / 使用</span>
      <span><svg width="46" height="14"><path d="M2 10 H40" stroke="#9aa4b2" stroke-width="1.4"
        stroke-dasharray="5 4"/><path d="M44 10 L34 5 L34 15" fill="none" stroke="#9aa4b2" stroke-width="1.4"/></svg> 依赖（虚线）</span>
      <span class="pill">黄框 = 注解 / 注意事项</span>
    </div>"""

    stats = """
    <div class="stats">
      <div><b>143</b><span>源文件（命名检查全过）</span></div>
      <div><b>2836</b><span>check.sh 检查项</span></div>
      <div><b>54</b><span>控件外观（default 主题）</span></div>
      <div><b>111 / 143</b><span>颜色 / 数值 token</span></div>
      <div><b>2</b><span>图形后端（GL / Vulkan）</span></div>
      <div><b>4</b><span>目标平台（macOS/Linux/Win/Switch）</span></div>
      <div><b>8.89 MB</b><span>Switch NRO 实测</span></div>
    </div>"""

    doc = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>bklv（BekUI）UML 图</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  :root {{
    --bg:#f6f8fb; --card:#ffffff; --ink:{INK}; --muted:{MUTED}; --line:#dbe1e8;
    --accent:{ACCENT}; --code:#eef2f7;
  }}
  @media (prefers-color-scheme: dark) {{
    :root {{ --bg:#12161d; --card:#1a1f28; --ink:#e7ecf3; --muted:#9aa4b2; --line:#2c3441;
             --accent:#5aa9e6; --code:#222833; }}
    svg text.cname, svg text.row, svg text.edge, svg text.msg, svg text.groupname {{ fill:#e7ecf3; }}
    svg text.stereo, svg text.dim, svg text.note, svg text.badge {{ fill:#a9b4c2; }}
    svg rect[fill="#ffffff"], svg rect[fill="#f2f5f9"], svg rect[fill="#f7f9fc"] {{ fill:#1e242e; }}
    svg rect[fill="#eef2f7"], svg rect[fill="#eaeef4"] {{ fill:#232b36; }}
      svg rect[fill="#e3f0fb"], svg rect[fill="#fdf1e0"], svg rect[fill="#e6f4e6"] {{ fill:#232b36; }}
    svg path[stroke="#9aa4b2"] {{ stroke:#4d5867; }}
    svg path[stroke="#c9d3df"], svg path[stroke="#dbe1e8"], svg path[fill="none"][stroke="#e2e7ee"] {{ stroke:#39424f; }}
    svg path[stroke="#9aa4b2"][fill="#ffffff"] {{ fill:#1e242e; }}
  }}
  * {{ box-sizing:border-box; }}
  body {{ margin:0; background:var(--bg); color:var(--ink);
    font:14px/1.6 ui-sans-serif,-apple-system,"PingFang SC","Microsoft YaHei",Segoe UI,Helvetica,Arial; }}
  header {{ padding:34px 28px 18px; max-width:1600px; margin:0 auto; }}
  h1 {{ margin:0 0 6px; font-size:26px; letter-spacing:.3px; }}
  header p {{ margin:4px 0; color:var(--muted); }}
  .meta {{ font-size:12.5px; color:var(--muted); }}
  .meta code {{ background:var(--code); padding:1px 6px; border-radius:5px; }}
  nav {{ position:sticky; top:0; z-index:9; backdrop-filter:blur(8px);
    background:color-mix(in srgb, var(--bg) 86%, transparent);
    border-bottom:1px solid var(--line); padding:10px 28px; display:flex; gap:8px;
    overflow-x:auto; white-space:nowrap; }}
  nav a {{ color:var(--ink); text-decoration:none; font-size:13px; padding:5px 11px;
    border:1px solid var(--line); border-radius:999px; background:var(--card); }}
  nav a:hover {{ border-color:var(--accent); color:var(--accent); }}
  main {{ max-width:1600px; margin:0 auto; padding:8px 28px 60px; }}
  section {{ background:var(--card); border:1px solid var(--line); border-radius:14px;
    padding:20px 22px 8px; margin:20px 0; box-shadow:0 1px 2px rgba(16,24,40,.04); }}
  h2 {{ margin:2px 0 4px; font-size:19px; }}
  .lead {{ margin:0 0 10px; color:var(--muted); }}
  figure {{ margin:6px 0 18px; }}
  figcaption {{ font-size:12.5px; color:var(--muted); margin:6px 2px 0; }}
  svg {{ display:block; max-width:100%; height:auto; }}
  svg text {{ font-size:11.5px; fill:{INK}; }}
  svg text.cname {{ font-size:12.5px; text-anchor:middle; }}
  svg text.stereo {{ font-size:10px; text-anchor:middle; fill:{MUTED}; font-style:italic; }}
  svg text.row {{ font-size:11px; }}
  svg text.row.dim, svg text.dim {{ fill:{MUTED}; font-size:10.5px; }}
  svg text.edge {{ font-size:10.5px; text-anchor:middle; fill:{MUTED}; }}
  svg text.note {{ font-size:11px; fill:#8a4b00; }}
  svg text.groupname {{ font-size:11px; fill:{MUTED}; font-weight:700; }}
  svg text.badge {{ font-size:9.5px; text-anchor:middle; fill:{ACCENT}; }}
  svg text.msg {{ font-size:10.5px; text-anchor:middle; fill:{ACCENT}; }}
  .legend {{ display:flex; flex-wrap:wrap; gap:18px; align-items:center; color:var(--muted);
    font-size:12.5px; margin:6px 0 14px; }}
  .legend span {{ display:flex; align-items:center; gap:6px; }}
  .pill {{ background:var(--code); padding:3px 10px; border-radius:999px; }}
  .stats {{ display:grid; grid-template-columns:repeat(auto-fit,minmax(180px,1fr)); gap:12px;
    margin:16px 0 4px; }}
  .stats div {{ background:var(--card); border:1px solid var(--line); border-radius:12px;
    padding:12px 14px; }}
  .stats b {{ display:block; font-size:20px; }}
  .stats span {{ color:var(--muted); font-size:12px; }}
  .cols {{ display:grid; grid-template-columns:repeat(auto-fit,minmax(320px,1fr)); gap:14px; }}
  .cols div {{ background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px 16px; }}
  .cols h3 {{ margin:2px 0 8px; font-size:14.5px; }}
  .cols ul {{ margin:6px 0 0 18px; padding:0; color:var(--muted); }}
  .cols li {{ margin:3px 0; }}
  code {{ background:var(--code); padding:1px 5px; border-radius:4px; font-size:12.5px; }}
  footer {{ max-width:1600px; margin:0 auto; padding:0 28px 40px; color:var(--muted); font-size:12.5px; }}
  @media print {{ nav {{ display:none; }} section {{ break-inside:avoid; box-shadow:none; }} }}
</style>
</head>
<body>
<header>
  <h1>bklv（BekUI）· 库结构与 UML 图</h1>
  <p>C++17 的 ImGui 风格组件库：一套上层代码，四个平台、两条图形后端。这份页面是<b>离线自包含</b>的
     （图全部是内联 SVG，不依赖任何外部脚本 / 字体）。</p>
  <p class="meta">生成方式：<code>python3 scripts/gen_uml.py</code> → <code>docs/uml.html</code>
     · 对应提交 <code>{html.escape(rev)}</code> · 图按代码里的实际继承关系画，改名记得重跑脚本</p>
  {stats}
</header>
<nav>{nav}</nav>
<main>
  {legend}
  {''.join(body)}
  <section id="cheat">
    <h2>8 · 读代码的几个入口</h2>
    <div class="cols">
      <div><h3>想看"一帧怎么跑"</h3><ul>
        <li><code>bklv/ui/app/app.cpp</code> 的 <code>App::frame()</code>（布局 → update → frame → Render）</li>
        <li><code>bklv/ui/view/view.cpp</code> 的 <code>View::frame()</code> / <code>draw()</code></li></ul></div>
      <div><h3>想看"控件长什么样"</h3><ul>
        <li>外观 <code>resources/theme/default/base|widget/*.json</code>（54 份）</li>
        <li>语义绘制 <code>bklv/ui/theme/appearance.cpp</code> 的 <code>drawCustomLayer</code></li></ul></div>
      <div><h3>想看"怎么加后端 / 平台"</h3><ul>
        <li><code>bklv/graphics/render/graphics.hpp</code>（RenderBackend 接口）</li>
        <li><code>notes/adding-render-backend.md</code>、<code>bklv/system/platform/</code></li></ul></div>
      <div><h3>想验收</h3><ul>
        <li><code>./scripts/check.sh</code>（2836 项）</li>
        <li>截图：<code>App::requestScreenshot()</code>（不要用系统录屏）</li></ul></div>
    </div>
  </section>
</main>
<footer>
  图例说明：菱形 = 组合（对象生命周期由持有方决定，如 App 持有 Platform / RenderBackend）；
  空心三角 = 继承（真正的 <code>: public</code> 关系）；虚线箭头 = 依赖（用到但不持有）。
  本页由 <code>scripts/gen_uml.py</code> 生成，改动要重新生成，不要手改 HTML。
</footer>
</body>
</html>
"""
    with open(path_out, "w", encoding="utf-8") as f:
        f.write(doc)
    print(f"已生成 {path_out}（{len(doc)/1024:.0f} KB）")


if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    build_html(os.path.join(root, "docs", "uml.html"))
