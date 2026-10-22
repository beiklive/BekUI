#!/usr/bin/env python3
"""生成 docs/manual.html：bklv 的单页手册（左侧目录 + 右侧内容，离线可用）。"""

import html
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent  # 仓库根
OUT = ROOT / 'docs' / 'manual.html'

# --------------------------------------------------------------------------
# 迷你 markdown 渲染（标题 / 表格 / 围栏代码 / 列表 / 引用 / 分隔线 / 行内标记）
# --------------------------------------------------------------------------

def inline(text: str) -> str:
    text = html.escape(text)
    text = re.sub(r'`([^`]+)`', r'<code>\1</code>', text)
    text = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', text)
    text = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'<a href="\2">\1</a>', text)
    return text


class Renderer:
    def __init__(self):
        self.toc = []      # [(level, id, title)]
        self.counter = 0

    def slug(self, title):
        self.counter += 1
        return f'sec-{self.counter}'

    def heading(self, level, text):
        return f'<h{level}>{inline(text)}</h{level}>'

    def render(self, md: str) -> str:
        lines = md.split('\n')
        out = []
        i = 0
        n = len(lines)

        while i < n:
            line = lines[i]

            # 围栏代码
            if line.strip().startswith('```'):
                lang = line.strip()[3:].strip()
                i += 1
                code = []
                while i < n and not lines[i].strip().startswith('```'):
                    code.append(lines[i])
                    i += 1
                i += 1
                cls = f' class="lang-{html.escape(lang)}"' if lang else ''
                out.append(f'<pre><code{cls}>{html.escape(chr(10).join(code))}</code></pre>')
                continue

            # 标题
            m = re.match(r'^(#{1,4})\s+(.*)$', line)
            if m:
                # 章节标题统一由本脚本占 h2；markdown 里的 # / ## 降到 h3，### 及更深降到 h4
                level = 3 if len(m.group(1)) <= 2 else 4
                out.append(self.heading(level, m.group(2)))
                i += 1
                continue

            # 分隔线
            if re.match(r'^\s*---+\s*$', line):
                out.append('<hr>')
                i += 1
                continue

            # 表格：表头行 + 分隔行
            if line.strip().startswith('|') and i + 1 < n and re.match(r'^\s*\|[\s:|-]+\|\s*$', lines[i + 1]):
                header = [c.strip() for c in line.strip().strip('|').split('|')]
                i += 2
                rows = []
                while i < n and lines[i].strip().startswith('|'):
                    rows.append([c.strip() for c in lines[i].strip().strip('|').split('|')])
                    i += 1
                out.append('<table><thead><tr>' + ''.join(f'<th>{inline(c)}</th>' for c in header) + '</tr></thead><tbody>')
                for row in rows:
                    out.append('<tr>' + ''.join(f'<td>{inline(c)}</td>' for c in row) + '</tr>')
                out.append('</tbody></table>')
                continue

            # 引用
            if line.startswith('>'):
                block = []
                while i < n and lines[i].startswith('>'):
                    block.append(lines[i].lstrip('>').strip())
                    i += 1
                out.append('<blockquote>' + inline(' '.join(block)) + '</blockquote>')
                continue

            # 无序列表
            if re.match(r'^\s*[-*]\s+', line):
                items = []
                while i < n and re.match(r'^\s*[-*]\s+', lines[i]):
                    items.append(re.sub(r'^\s*[-*]\s+', '', lines[i]))
                    i += 1
                out.append('<ul>' + ''.join(f'<li>{inline(t)}</li>' for t in items) + '</ul>')
                continue

            # 有序列表
            if re.match(r'^\s*\d+\.\s+', line):
                items = []
                while i < n and re.match(r'^\s*\d+\.\s+', lines[i]):
                    items.append(re.sub(r'^\s*\d+\.\s+', '', lines[i]))
                    i += 1
                out.append('<ol>' + ''.join(f'<li>{inline(t)}</li>' for t in items) + '</ol>')
                continue

            # 空行
            if not line.strip():
                i += 1
                continue

            # 段落（连续的普通行）
            para = []
            while i < n and lines[i].strip() and not re.match(
                    r'^(#{1,4}\s|\s*[-*]\s|\s*\d+\.\s|>|\s*\||```|\s*---+\s*$)', lines[i]):
                para.append(lines[i].strip())
                i += 1
            out.append('<p>' + inline(' '.join(para)) + '</p>')

        return '\n'.join(out)


R = Renderer()

# --------------------------------------------------------------------------
# 采集仓库事实
# --------------------------------------------------------------------------

def run(*cmd):
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True).stdout.strip()


def layer_stats():
    rows = []
    for layer, purpose in [
        ('core', '接口层：View/Box、App 帧循环、输入焦点、外观与主题、图形与平台抽象、解码、i18n、任务、全局状态'),
        ('views', '语义控件：Label / Image / ScrollingFrame / RecyclerFrame / Padding / DebugLogView'),
        ('widgets', '外观驱动控件：Shell / Header / BottomBar / TabBar / Button / Cell 家族 / Dialog …'),
        ('compile', '解析层：JSON → 视图树 / 外观 / 主题（两段式模板）'),
        ('backends', '图形后端：OpenGL 3.2 Core / Vulkan / OpenGL ES 3'),
        ('platforms', '平台实现：SDL3（桌面三平台）/ libnx（Switch）、手柄与 IME'),
    ]:
        cpp = len(list((ROOT / 'bklv' / layer).glob('*.cpp')))
        hpp = len(list((ROOT / 'bklv' / layer).glob('*.hpp')))
        rows.append((layer, cpp, hpp, purpose))
    return rows


def header_classes(layer):
    """(类名, 头文件, 一句话说明) —— 说明取文件顶部注释里第一句像描述的话。"""
    items = []
    for path in sorted((ROOT / 'bklv' / layer).glob('*.hpp')):
        text = path.read_text(encoding='utf-8', errors='ignore')
        classes = []
        for m in re.finditer(r'^class\s+(\w+)\s*(?::|$)', text, re.M):
            name = m.group(1)
            if name in classes or name in ('Impl', 'TextureRecord'):
                continue
            classes.append(name)
        if not classes:
            continue
        desc = ''
        for line in text.split('\n')[:40]:
            line = line.strip()
            if not line.startswith('//'):
                continue
            body = line.lstrip('/').strip()
            if not body or body.startswith('=') or body.startswith('-') or len(body) < 6:
                continue
            if body.startswith('对应') or body.startswith('为什么'):
                continue
            desc = body
            break
        items.append((classes[0], ' / '.join(classes[1:]), path.name, desc))
    return items


def view_types():
    """JSON 里能写的 "type"：内置 + 控件层。"""
    names = []
    for rel in ('bklv/compile/view_parse.cpp', 'bklv/widgets/widget_types.cpp'):
        text = (ROOT / rel).read_text(encoding='utf-8', errors='ignore')
        for m in re.finditer(r'registerViewType\("([^"]+)"', text):
            if m.group(1) not in names:
                names.append(m.group(1))
    return names


def flatten(obj, prefix=''):
    out = []
    for k, v in obj.items():
        if k.startswith('$'):
            continue
        key = f'{prefix}{k}' if not prefix else f'{prefix}.{k}'
        if isinstance(v, dict):
            out += flatten(v, key)
        else:
            out.append((key, v))
    return out


def token_tables():
    sections = []
    for name, file, note in [
        ('颜色', 'colors.json', '值就是 CSS 颜色；`color.<组>.<名>` 在外观 JSON 里用 `@theme/color.组.名` 引用'),
        ('数值', 'metrics.json', '尺寸/字号按 1280×720 设计稿写，取值时乘 `uiScale`（`anim.*` 毫秒与 `font.lineHeight` 系数不缩放）'),
    ]:
        data = json.loads((ROOT / f'resources/theme/default/tokens/{file}').read_text(encoding='utf-8'))
        keys = flatten(data)
        grouped = {}
        for key, value in keys:
            head = key.split('.')[0]
            grouped.setdefault(head, []).append((key, value))
        body = [f'<p>{inline(note)}。共 <b>{len(keys)}</b> 个键。</p>']
        for head, pairs in grouped.items():
            body.append(f'<h4>{html.escape(head)}</h4>')
            body.append('<table><thead><tr><th>键</th><th>default 值</th></tr></thead><tbody>')
            for key, value in pairs:
                body.append(f'<tr><td><code>{html.escape(key)}</code></td><td><code>{html.escape(str(value))}</code></td></tr>')
            body.append('</tbody></table>')
        sections.append((name, '\n'.join(body)))
    return sections


# --------------------------------------------------------------------------
# 各章节
# --------------------------------------------------------------------------

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


md_api = read('docs/bklv-api.md')
md_json = read('docs/json-syntax.md')

commit = run('git', 'rev-parse', '--short', 'HEAD')

sections = []

# 1 概览
counts = {d: len(list((ROOT / 'bklv' / d).glob('*.cpp'))) + len(list((ROOT / 'bklv' / d).glob('*.hpp')))
          for d in ('core', 'views', 'widgets', 'compile', 'backends', 'platforms')}
sections.append(('概览', f'''
<p><b>bklv</b> 是一个跨平台 C++17 界面库：<b>Yoga 算 Flexbox 布局</b>，<b>ImGui 的 ImDrawList 负责绘制</b>。
目标形态是能被外部项目 <code>add_subdirectory</code> / <code>find_package</code> 引入的库，本仓库同时提供一个 demo 程序。</p>

<table>
<thead><tr><th>项目</th><th>内容</th></tr></thead>
<tbody>
<tr><td>平台</td><td>macOS / Linux / Windows（SDL3）、Nintendo Switch（libnx + EGL/GLES3）</td></tr>
<tr><td>图形后端</td><td>OpenGL 3.2 Core、Vulkan、OpenGL ES 3（编译期选择，所有功能都要双后端验证）</td></tr>
<tr><td>主题</td><td>两套：<code>default</code>（纯扁平）与 <code>ios</code>（苹果风：系统色 / 分组列表 / 大标题 / 底部 Tab 栏 / 玻璃上下栏）</td></tr>
<tr><td>界面描述</td><td>C++ 建树，或 JSON 视图描述（<code>template</code> / <code>instantiate</code> 两段式）</td></tr>
<tr><td>不做</td><td>XML 布局与绑定、视频、PS4/PSV（音频由应用层负责，库里只有音效合成）</td></tr>
</tbody>
</table>

<p>库的代码规模（不含 <code>3rd/</code> 第三方）：{'、'.join(f'<code>{k}/</code> {v} 文件' for k, v in counts.items())}。</p>

<h3>仓库结构</h3>
<pre><code>bklv/                库本体（自包含：代码 + 3rd/ + 自己的 CMakeLists.txt）
  bklv.hpp           伞头：常用类型一次引入（demo 只 include 它）
  core/  views/  widgets/  compile/  backends/  platforms/
demo/                展示程序（15 个页面 + 底部导航 + 液态玻璃）

resources/theme/     主题 JSON（default / ios）
scripts/             check.sh（一次跑完命名/分层/双后端零警告构建）
docs/                文档（本手册就是其中一份）</code></pre>
'''))

# 2 分层
layer_rows = ''.join(
    f'<tr><td><code>{name}/</code></td><td>{cpp}</td><td>{hpp}</td><td>{html.escape(p)}</td></tr>'
    for name, cpp, hpp, p in layer_stats())
sections.append(('分层架构', f'''
<p>层 = <code>bklv/</code> 下的第一级目录。所有层编进同一个 OBJECT 库、共用一个 include 根，
所以"分层"是目录约定而不是编译单元边界 —— 靠 <code>scripts/check_layers.py</code> 门禁维持。</p>

<table>
<thead><tr><th>层</th><th>.cpp</th><th>.hpp</th><th>职责</th></tr></thead>
<tbody>{layer_rows}</tbody>
</table>

<h3>允许的依赖方向</h3>
<pre><code>widgets   → views / core / compile     （视图与外观驱动的控件在下游）
views     → core                       （语义控件只认 ImDrawList 与 core 接口）
compile   → core                       （解析实现用到 core 的数据类型）
backends  → core                       （后端实现 core 的接口，含 core/glass）
platforms → core / backends            （平台创建并持有后端）
core      → platforms                  （工厂：方向由"编译期裁剪 + 工厂"反转，属预期）
任何一层  → 3rd                        （第三方是叶子）</code></pre>

<p>禁止的方向：<code>core → widgets / views / compile / backends</code>、<code>compile → views / widgets</code>、
<code>views → widgets</code>。存量的 4 条反向依赖记在 <code>check_layers.py</code> 的 BASELINE 里（白名单必须精确：
消解一条就要删一行，脚本会报"白名单已过期"）。</p>

<h3>两条机械门禁</h3>
<ul>
<li><code>python3 scripts/check_naming.py</code> —— 枚举值 UPPER_SNAKE、类型 PascalCase、方法/自由函数 camelCase、布尔开关 <code>set&lt;名词&gt;Enabled</code>、状态查询 <code>is*/has*/can*</code>。</li>
<li><code>python3 scripts/check_layers.py</code> —— 禁止新增反向依赖；另外查<b>伞头可达范围</b>：
<code>bklv/bklv.hpp</code> 顺着 include 不许碰到 <code>backends/</code> / <code>platforms/</code> 或 <code>&lt;vulkan/vulkan.h&gt;</code>。</li>
</ul>

<h3>全局状态（core/globals.hpp）</h3>
<p>进程级状态只有一处登记：<code>bklv::globals::getGlobalStore()</code>。对象槽分"拥有（obtain 懒创建 / set owned）"与
"借用（App、平台、后端）"，另有数值槽（number / flag / text）与重置钩子；<code>resetGlobals()</code> 一轮清干净。
已登记：<code>EventBus</code>、<code>InputManager</code>、当前 <code>App</code>、玻璃服务，以及纹理仓库与四条注册表的重置钩子。
库仍是<b>单实例</b>（一个进程一套界面、全局 ImGui 上下文）。</p>
'''))

# 3 构建与运行
sections.append(('构建与运行', f'''
<pre><code># GL 版（产物到 dist/）
cmake --preset gl && cmake --build --preset gl

# Vulkan 版（产物到 dist/vk/，需要 Vulkan SDK / MoltenVK）
cmake --preset vk && cmake --build --preset vk

# 一次跑完所有门禁（命名 → 分层 → GL/VK 双后端零警告构建）
bash scripts/check.sh</code></pre>

<p>约定：<code>-Wall -Wextra -Wpedantic</code> <b>零警告</b>；每个后端各一个构建目录、产物分目录（<code>dist/</code> 与 <code>dist/vk/</code>）。
CI 见 <code>.github/workflows/ci.yml</code>：静态检查 + {{Ubuntu, macOS}} × {{GL, Vulkan}} 构建，
构建出现 warning 即失败（Windows 还没接，那条路要 MSYS2 + <code>build.bat</code>）。</p>

<h3>用到自己的项目里</h3>
<pre><code>add_subdirectory(third_party/bklv)
target_link_libraries(my_app PRIVATE bklv::bklv)

#include "bklv/bklv.hpp"
auto* app = new bklv::App();
app->setRootView(new bklv::Shell());
app->run("我的应用", 1280, 720);</code></pre>
<p>包含路径不用自己加：<code>bklv::bklv</code> 会把 <code>bklv/</code> 的父目录带出来，所以直接写
<code>#include "bklv/core/app.hpp"</code>。图形后端是编译期决定的（<code>-DBKLV_GRAPHICS=gl|vk</code>）。</p>
'''))

# 4 接口速查（docs/bklv-api.md）
sections.append(('接口速查', R.render(md_api)))

# 5 内置控件
def widget_tables():
    out = []
    for layer, title in (('views', '语义控件（views/）'), ('widgets', '外观驱动控件（widgets/）')):
        rows = ''.join(
            f'<tr><td><code>{html.escape(cls)}</code>{(" <span class=muted>" + html.escape(rest) + "</span>") if rest else ""}</td>'
            f'<td><code>{html.escape(file)}</code></td>'
            f'<td>{inline(desc)}</td></tr>'
            for cls, rest, file, desc in header_classes(layer))
        out.append(f'''
<h3>{title}</h3>
<table>
<thead><tr><th>类型</th><th>头文件</th><th>说明</th></tr></thead>
<tbody>{rows}</tbody>
</table>''')
    return '\n'.join(out)


sections.append(('内置控件', f'''
<p>控件分两类：<code>views/</code> 是<b>语义控件</b>（只认 ImDrawList 与 core 接口，语义直白）；
<code>widgets/</code> 是<b>外观驱动控件</b>（继承 <code>AppearanceView</code>，绘制结构、状态动效都来自主题 JSON，
代码只写行为）。下面按头文件列出全部公开类型。</p>
{widget_tables()}

<h3>几个常用控件的要点</h3>
<table>
<thead><tr><th>控件</th><th>要点</th></tr></thead>
<tbody>
<tr><td><code>Shell</code></td><td>四层外壳（内容 / 覆盖 / 背景 / Hud）：把 <code>Header</code>、<code>BottomBar</code> 挂到覆盖层，内容进内容层；<code>pushContentView</code> 做下钻。<b>顶栏/底栏只是一块区域 —— 任何主题都不许给它画底色或边框</b>（test 钉住）。</td></tr>
<tr><td><code>Header</code></td><td>结构 <code>[图标][标题][右侧区]</code>；<code>setLargeTitle()</code> + <code>setCollapseSource(scroller)</code> 做大标题随滚动收起（只改绘制期量，零重排）。</td></tr>
<tr><td><code>TabBar</code> / <code>Sidebar</code> / <code>TabFrame</code></td><td>底部导航条（A/点击才切页，横向可滚动）/ 左侧栏（聚焦即切页，Switch 语义）/ tab 栏 + 内容区。</td></tr>
<tr><td><code>Button</code> / Cell 家族</td><td>外观驱动；<code>setOnClick</code> 接行为，另外可用 <code>registerAction(键, 提示文本, listener)</code> 挂手柄键。</td></tr>
<tr><td><code>RecyclerFrame</code></td><td>对象池虚拟列表：单段只写 <code>getItemCount()</code>；分段写 <code>getSectionCount()</code> 那一套。JSON 侧由 <code>RecyclerView</code> 桥接（<code>data</code>/<code>itemBinder</code>/<code>onEnd</code> 只写名字）。</td></tr>
<tr><td><code>Dialog</code> / <code>Dropdown</code> / <code>EditTextDialog</code></td><td>都走 <code>App::pushOverlay/popOverlay</code> 的模态栈：焦点圈在弹层内、关闭后回到原焦点。</td></tr>
</tbody>
</table>
'''))

# 6 主题与外观
color_tbl, metric_tbl = token_tables()[0][1], token_tables()[1][1]
sections.append(('主题与外观', f'''
<p>两套主题，都在 <code>resources/theme/&lt;名字&gt;/</code> 下：<code>theme.json</code>（显式 <code>files[]</code> 清单，不扫目录）
+ <code>tokens/</code>（colors / metrics）+ <code>base|widget|view/</code> 外观。复制一套改名改 JSON 就是新皮肤；
<code>themes.json</code> 的 <code>available</code> 列出可选主题，运行时 <code>App::setTheme("ios")</code> 切换。</p>

<table>
<thead><tr><th>主题</th><th>定位</th><th>关键做法</th></tr></thead>
<tbody>
<tr><td><code>default</code></td><td>纯扁平</td><td>无阴影、无玻璃、无渐变；靠 1px 描边分面；<code>color.common.shadow</code> 全透明（代码驱动的软阴影也随之关闭）</td></tr>
<tr><td><code>ios</code></td><td>苹果风</td><td>iOS dark 系统色；分组列表（行内缩分隔线）；胶囊分段；<code>glass_surface</code> 用 #1C1C1ECC 的 material；大标题 + 底部 Tab 栏</td></tr>
</tbody>
</table>

<h3>外观 JSON 的四段</h3>
<pre><code>{{
  "import": "base/box",          // 先继承一份，再逐字段合并（可变体可以用后缀拼名字）
  "box":   /* 尺寸/内边距/圆角/边框（影响布局）*/,
  "layers": [                    // 绘制结构，按顺序画
    {{ "op": "rect",  "id": "bg",   "fill": "@theme/color.surface.card", "radius": 10 }},
    {{ "op": "text",  "id": "title", "color": "@theme/color.text.primary" }},
    {{ "op": "glass", "id": "glass", "fill": "#1C1C1ECC" }}
  ],
  "states": {{
    "focused": {{
      // self = 控件自身：视图级通道（translateX/Y、scaleX/Y、alpha、rotate、skewX/Y）
      "self": {{ "translateY": {{ "to": -4, "duration": 160, "easing": "cubicOut" }} }},
      // 层目标可以分组写；旧写法 "bg.fill" 仍兼容
      "bg":   {{ "fill": {{ "to": "@theme/color.highlight.background", "duration": 120 }} }}
    }}
  }},
  "transitions": {{ "enter": {{ ... }} }}
}}</code></pre>

<p>六种内置层：<code>rect</code> / <code>text</code> / <code>image</code> / <code>shadow</code> / <code>glass</code> / <code>custom</code>；
第三方可以注册自己的层（<code>registerLayer&lt;T&gt;()</code> 一行，参数模式声明一次，解析/缩放/设计器候选键自动跟上）。
<b>层序</b>：内置层按 JSON 书写顺序画（只有阴影、玻璃被提到最前）；声明 DECORATION / OVERLAY 的扩展层画在所有内置层之上。
<b>高亮底必须排在内容层之前</b>（<code>hlBg</code> 在 text/image 之前）；聚焦描边由浮层的焦点框绘制，
外观里不再需要 <code>hlGlow</code>/<code>hlBorder</code> 占位层。</p>

<h3>主题数值的"计算"规则</h3>
<ul>
<li>尺寸/字号按 <b>1280×720</b> 设计稿写，运行时 <code>uiScale = 画布宽 / 1280</code>；<code>metric.*</code> 取值时乘缩放（<code>anim.*</code> 毫秒、<code>font.lineHeight</code> 系数不缩放）。</li>
<li>外观 JSON 里的 <code>radius / strokeWidth / width / height / fontSize / inset / offset</code> 会乘缩放；<code>opacity</code>（系数）与 <code>duration</code>（毫秒）不缩放。</li>
<li><b>代码里写死的像素值不缩放</b> —— 想跟随比例就用主题 metric，或把数字写进外观 JSON。</li>
<li>主题只覆盖"没显式设置过的值"：代码里 <code>setCornerRadius(r, explicit=true)</code> 之类的显式值优先于外观。</li>
<li><code>@theme/</code> 引用写错会硬失败（不静默回退成洋红）；主题 JSON 的 <code>files[]</code> 少写一项会以"主题里没有控件外观"静默回退。</li>
</ul>

<h3>颜色 token（default 主题，共 113 个键）</h3>
{color_tbl}

<h3>数值 token（default 主题）</h3>
{metric_tbl}
'''))

# 7 JSON 视图系统
sections.append(('JSON 视图与字段', f'''
<p>JSON 描述视图树：<code>"type"</code> 指定类型、其余键是属性、<code>"children"</code> 是子节点、<code>"include"</code> 引别的文件。
两段式：<code>ViewTemplate</code>（可缓存、与语言无关）→ <code>instantiate()</code>（解析 <code>@i18n/</code> 等）。
结构错误返回 <code>nullptr</code> + 日志；<b>未知属性只警告</b>（带拼错提示）。上限：深度 32 / 节点 4096 / include 栈 32。</p>

<h3>属性值的写法与求值时机</h3>
<table>
<thead><tr><th>写法</th><th>含义</th><th>求值时机</th></tr></thead>
<tbody>
<tr><td><code>12</code> / <code>"12"</code> / <code>"12px"</code></td><td>像素（按 720p 设计稿，运行时乘 uiScale）</td><td>解析期</td></tr>
<tr><td><code>"50%"</code></td><td>相对父容器的百分比（宽高与绝对定位四边）</td><td>布局期</td></tr>
<tr><td><code>"auto"</code></td><td>由内容决定（只有 width / height）</td><td>布局期</td></tr>
<tr><td><code>@theme/color.a.b</code></td><td>主题颜色</td><td>实例化期</td></tr>
<tr><td><code>@style/metric.a.b</code></td><td>主题数值</td><td>实例化期</td></tr>
<tr><td><code>@i18n/key</code></td><td>文案（<code>resources/i18n/&lt;locale&gt;.json</code>）</td><td>实例化期</td></tr>
<tr><td><code>@res/x</code></td><td>文件路径，相对 <code>resources/</code></td><td>使用处自己解析</td></tr>
<tr><td><code>"bind": "字段名"</code></td><td>数据绑定：写到该类型的主属性（Label→text，Image→src）；别的类型要显式 <code>"bindTo"</code></td><td>列表项 setIndex 时</td></tr>
</tbody>
</table>
<p>能写的 <code>"type"</code>（注册表实测）：{'、'.join(f'<code>{n}</code>' for n in view_types())}。</p>

{R.render(md_json)}
'''))

# 8 运行期能力
sections.append(('运行期能力', '''
<h3>帧预算：界面永不卡，任务可以晚</h3>
<p><code>App::setFrameBudgetMs(4)</code>（默认 4ms）给"可延后工作"一个每帧时间预算：
后台回调与 delay 到期任务、纹理上传、动图换帧、SVG 栅格化、纹理缓存淘汰 —— 用完就停手、下一帧继续，
且<b>不丢活</b>（没跑的回调放回队首、到期延迟放回表里）。布局 / update 业务 / 输入 / 绘制不吃预算。</p>
<pre><code>const FrameTimeSummary s = app.getFrameStats().getSummary();
// p50 / p95 / p99 / 最坏帧；droppedCount = 超过 16.67ms（60Hz 一帧）的帧数
// 每个 App::run() 程序退出时还会自动打一行帧时间流水</code></pre>

<h3>任务与线程池（core/task.hpp）</h3>
<pre><code>// 固定大小线程池（默认 2 worker），回调一定回到主线程执行
task::run(work, done);
task::runResult([path] { return decodeImageFile(path.c_str()); },
                [this](DecodedImage image) { this->upload(image); });
task::postToMain(callback);              // 工作线程里调用也安全
task::delay(1000, [] { /* 主线程，1 秒后 */ });
// 对象先死的情况用 core/alive.hpp 的 AliveFlag（weak_ptr）护栏</code></pre>

<h3>图片：异步解码 + 背压</h3>
<ul>
<li><code>Image::setImageFromFileAsync(path)</code>：后台解码（线程池）→ 回主线程上传；<code>setDecodePriority()</code> 越小越先。</li>
<li><code>pumpDecodeQueue()</code> 按 <code>getDecodeConcurrency()</code>（默认 4）限制在飞解码数；<code>pumpUploads()</code> 按
<code>getMaxUploadsPerFrame()</code>（默认 2）+ 帧预算消费上传。</li>
<li>滚出视口/换图：<code>cancelDecode(key)</code> 直接丢弃，不占上传配额；同一路径不重复解码。</li>
<li>纹理预算：<code>setTextureBudget(n)</code> + <code>trimTextureCache()</code>（超预算淘汰 refCount==0 的 LRU，每帧增量收）。</li>
<li><b>帧内同步加载是违规</b>：<code>setImageFromFile()</code> 在帧内被调用会打一条节流警告。</li>
</ul>

<h3>输入 → 动作绑定</h3>
<pre><code>// 键盘 Enter/Space、手柄 A、鼠标点击都汇到同一个动作
button->setOnClick([this] { this->save(); });
// 额外挂一个键（hintText 就是底栏提示栏的来源，hidden=true 则不显示）
button->registerAction(ControllerButton::Y, "删除", [this](View*) { this->remove(); });
// 系统键（不吃焦点链、优先级高于模态）
getInputManager().setSystemAction(ImGuiKey_F1, [this] { /* 开关调试层 */ });</code></pre>
<p>派发链：<code>InputManager::update()</code> → 导航 / 动作 → <code>View::triggerAction(键)</code> 沿父链找谁注册过；
有模态时只在模态子树内派发。鼠标点击走同一条动作，但默认"只选中不执行"（<code>onPointerAction=false</code>，
防止点列表项误触发）——Button / Cell / Slider / Dropdown / TabFrame 把它置成了 true。</p>

<h3>其它常用能力</h3>
<table>
<thead><tr><th>能力</th><th>入口</th></tr></thead>
<tbody>
<tr><td>事件总线</td><td><code>getEventBus().subscribe&lt;事件&gt;(handler)</code>（RAII 句柄，析构自动退订）</td></tr>
<tr><td>通知 / Toast</td><td><code>App::notify(text, seconds)</code></td></tr>
<tr><td>弹层</td><td><code>App::pushOverlay(view)</code> / <code>popOverlay(view)</code>（模态输入 + 焦点恢复）</td></tr>
<tr><td>i18n</td><td><code>translate("a/b")</code>（或 <code>using namespace bklv::i18n;</code> 后写 <code>"a/b"_i18n</code>），文案在 <code>resources/i18n/&lt;locale&gt;.json</code>；切语言要显式重译</td></tr>
<tr><td>音效</td><td><code>audio::play(Sound::CLICK)</code>（8 种音效是代码合成的）</td></tr>
<tr><td>截图</td><td><code>App::requestScreenshot(path, delayFrames)</code>（离屏读后缓冲，不需要系统录屏权限）</td></tr>
<tr><td>主题切换</td><td><code>App::setTheme("ios")</code>（重新套用外观 + 重排一次）</td></tr>
</tbody>
</table>
'''))

# 9 约定与坑
sections.append(('约定与已知坑', '''
<ul>
<li><b>零警告</b>：<code>-Wall -Wextra -Wpedantic</code>，构建出现一个 warning 就算失败（CI 也这么判）。</li>
<li><b><code>update()</code> 路径不许读 <code>ImGui::GetIO()</code></b>：无窗口时没有上下文会空指针；时间用 <code>core/time.hpp</code>。</li>
<li><b>布局是延迟的</b>：<code>invalidate()</code> 只标脏，真正重算在下一帧；不要"改完立刻读尺寸"。
静态页面布局次数应为 0，排查用 <code>BKLV_FORCE_RELAYOUT=1</code>。</li>
<li><b>回收列表 cell 必须重写 <code>setSectionIndex</code></b>：分段路径走它，只重写 <code>setIndex</code> 会让绑定函数永不触发（列表整条空白）。</li>
<li><b>显式值优先</b>：<code>setCornerRadius(r, explicit=true)</code> 之类不会被外观 JSON 覆盖；外观套用一律 <code>explicit=false</code>。</li>
<li><b>高亮底顺序</b>：<code>hlBg</code> 必须在内容层之前，否则聚焦时整块不透明底把内容盖住（开关/单选曾整片消失）。</li>
<li><b>玻璃层顺序</b>：阴影(0) → 玻璃(1) → 其余(2)；玻璃不能排到第一个，否则半透明阴影把它压暗。</li>
<li><b>顶栏/底栏只是一块区域</b>：任何主题都不给它画底色或边框（排反了会多出一道横线）。</li>
<li><b>输入 → 响应同帧生效</b>：<code>App::frame</code> 的次序是"布局 → 输入 → 绘制"，别把输入挪到绘制之后。</li>
<li><b>帧内同步加载图片是违规</b>：批量/滚动场景用 <code>setImageFromFileAsync</code>。</li>
<li><b>JSON 里没有通用回调</b>：只能写"名字"（<code>data</code> / <code>itemBinder</code> / <code>onEnd</code> / <code>bind</code>），
行为必须在 C++ 注册（结构归 JSON、数据与行为归 C++）。<code>Box</code> 也不能在 JSON 里设可聚焦。</li>
</ul>
'''))

# 10 验收
sections.append(('验收与 CI', '''
<pre><code>scripts/check.sh 一共三步，任何一步失败就停：
  ① python3 scripts/check_naming.py     接口命名约定
  ② python3 scripts/check_layers.py     反向依赖 + 伞头可达范围
  ③ cmake GL / VK 双后端构建             零警告</code></pre>
<p>渲染路径（后端本体、平台层、demo 页面）仍靠人工验收 —— 这部分是已知盲区。
要验证某条行为就写一个临时程序（无窗口，自己 include <code>bklv/</code> 的头）跑一遍。</p>
'''))

# --------------------------------------------------------------------------
# 组装 HTML
# --------------------------------------------------------------------------

body_parts = []
for title, content in sections:
    body_parts.append(f'<section>{R.heading(2, title)}\n{content}\n</section>')
body_html = '\n'.join(body_parts)

# 目录：组装完再按**文档顺序**统一给标题编号（避免 markdown 渲染时机影响目录顺序）
collected = []


def add_anchors(match):
    tag, text = match.group(1), match.group(2)
    anchor = f'sec-{len(collected) + 1}'
    collected.append((int(tag[1]), anchor, re.sub(r'<[^>]+>', '', text).strip()))
    return f'<{tag} id="{anchor}">{text}</{tag}>'


body_html = re.sub(r'<(h[234])>(.*?)</\1>', add_anchors, body_html, flags=re.S)
toc_items = [(level, anchor, title) for level, anchor, title in collected if level in (2, 3, 4)]


def toc_html():
    out = ['<ul class="toc">']
    stack = []  # 已开的层级
    for level, anchor, title in toc_items:
        # 只在"层级变浅"时关列表：同一层的相邻小节共用同一个 <ul>
        while stack and stack[-1] > level:
            out.append('</ul></li>')
            stack.pop()
        if level == 2:
            out.append(f'<li><a href="#{anchor}">{html.escape(title)}</a></li>')
        else:
            if not stack or stack[-1] < level:
                out.append('<li class="sub"><ul>')
                stack.append(level)
            out.append(f'<li><a href="#{anchor}">{html.escape(title)}</a></li>')
    while stack:
        out.append('</ul></li>')
        stack.pop()
    out.append('</ul>')
    return '\n'.join(out)


stamp = run('date', '+%Y-%m-%d %H:%M')

HTML = f'''<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>bklv 手册 · 架构 / 接口 / 控件 / JSON</title>
<style>
:root {{
  --bg: #0b0d10; --panel: #121519; --panel-2: #171b21; --line: #242a33;
  --text: #e8ecf2; --dim: #a8b2c0; --muted: #6f7b8b;
  --accent: #0a84ff; --accent-dim: #0a84ff33; --code-bg: #0e1116; --radius: 10px;
}}
* {{ box-sizing: border-box; }}
html, body {{ margin: 0; padding: 0; }}
body {{
  background: var(--bg); color: var(--text);
  font: 15px/1.75 -apple-system, "SF Pro Text", "PingFang SC", "Hiragino Sans GB", "Microsoft YaHei", system-ui, sans-serif;
  -webkit-font-smoothing: antialiased;
}}
a {{ color: var(--accent); text-decoration: none; }}
a:hover {{ text-decoration: underline; }}
code {{ font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace; font-size: .9em;
  background: var(--code-bg); border: 1px solid var(--line); border-radius: 6px; padding: .1em .35em; color: #cfe3ff; }}
pre {{ background: var(--code-bg); border: 1px solid var(--line); border-radius: var(--radius);
  padding: 14px 16px; overflow: auto; }}
pre code {{ background: none; border: none; padding: 0; font-size: .88em; line-height: 1.65; color: #d7e3f4; }}
h1, h2, h3, h4 {{ line-height: 1.3; margin: 0; }}
h2 {{ font-size: 24px; margin: 0 0 16px; padding-bottom: 10px; border-bottom: 1px solid var(--line); }}
h3 {{ font-size: 18px; margin: 28px 0 10px; }}
h4 {{ font-size: 15px; margin: 20px 0 8px; color: var(--dim); }}
p {{ margin: 10px 0; }}
ul, ol {{ margin: 10px 0; padding-left: 22px; }}
li {{ margin: 4px 0; }}
hr {{ border: none; border-top: 1px solid var(--line); margin: 28px 0; }}
blockquote {{ margin: 12px 0; padding: 8px 14px; border-left: 3px solid var(--accent);
  background: var(--panel); border-radius: 0 var(--radius) var(--radius) 0; color: var(--dim); }}
table {{ border-collapse: collapse; width: 100%; margin: 12px 0 16px; font-size: .93em; }}
th, td {{ border: 1px solid var(--line); padding: 7px 10px; text-align: left; vertical-align: top; }}
th {{ background: var(--panel-2); color: var(--dim); font-weight: 600; white-space: nowrap; }}
tbody tr:nth-child(even) {{ background: #0f1216; }}
.muted {{ color: var(--muted); font-size: .85em; }}

/* 两栏 */
.wrap {{ display: grid; grid-template-columns: 300px minmax(0, 1fr); min-height: 100vh; }}
aside {{
  position: sticky; top: 0; align-self: start; height: 100vh; overflow: auto;
  background: var(--panel); border-right: 1px solid var(--line); padding: 18px 14px 40px;
}}
.brand {{ display: flex; align-items: baseline; gap: 8px; margin: 0 6px 6px; }}
.brand b {{ font-size: 20px; letter-spacing: .5px; }}
.brand span {{ color: var(--muted); font-size: 12px; }}
.version {{ margin: 0 6px 14px; color: var(--muted); font-size: 12px; }}
.search {{ width: 100%; margin: 0 0 12px; padding: 8px 10px; border-radius: 8px;
  border: 1px solid var(--line); background: var(--code-bg); color: var(--text); font-size: 13px; }}
.search:focus {{ outline: none; border-color: var(--accent); }}
.toc, .toc ul {{ list-style: none; margin: 0; padding: 0; }}
.toc > li {{ margin: 2px 0; }}
.toc > li > a {{ display: block; padding: 6px 8px; border-radius: 8px; color: var(--text); font-weight: 600; }}
.toc > li > a:hover {{ background: var(--panel-2); text-decoration: none; }}
.toc li.sub > ul {{ margin: 2px 0 8px 8px; border-left: 1px solid var(--line); }}
.toc li.sub > ul ul {{ margin: 2px 0 4px 8px; }}
.toc ul a {{ display: block; padding: 3px 10px; color: var(--dim); font-size: 13px; border-radius: 6px; }}
.toc ul a:hover {{ color: var(--text); background: var(--panel-2); text-decoration: none; }}
.toc a.active {{ background: var(--accent-dim); color: #dceaff; }}
main {{ padding: 34px 48px 120px; max-width: 1080px; }}
section {{ margin: 0 0 46px; scroll-margin-top: 20px; }}
.hint {{ color: var(--muted); font-size: 13px; margin-top: 10px; }}
footer {{ color: var(--muted); font-size: 12px; border-top: 1px solid var(--line); padding-top: 14px; }}
.top {{ position: fixed; right: 22px; bottom: 22px; background: var(--panel-2); border: 1px solid var(--line);
  border-radius: 999px; width: 42px; height: 42px; color: var(--dim); cursor: pointer; display: none; }}
@media (max-width: 900px) {{
  .wrap {{ grid-template-columns: 1fr; }}
  aside {{ position: static; height: auto; border-right: none; border-bottom: 1px solid var(--line); }}
  main {{ padding: 22px 18px 90px; }}
}}
@media print {{
  aside, .top {{ display: none; }}
  .wrap {{ grid-template-columns: 1fr; }}
  body {{ background: #fff; color: #111; }}
  pre, code, th, td {{ border-color: #bbb; }}
}}
</style>
</head>
<body>
<div class="wrap">
<aside>
  <div class="brand"><b>bklv</b><span>手册</span></div>
  <div class="version">提交 {commit} · {stamp} · 两套主题（default / ios）</div>
  <input class="search" id="search" type="search" placeholder="过滤目录（比如 json / 布局 / 控件）">
  {toc_html()}
</aside>
<main>
<h1>bklv 使用手册</h1>
<p class="hint">这份页面由仓库内容自动生成：架构、接口、控件清单、JSON 字段与约定。
左侧目录可过滤；建议在桌面浏览器里看（<code>docs/manual.html</code>，离线可用、无外部依赖）。</p>
{body_html}
<footer>生成自仓库提交 <code>{commit}</code>；架构与约定以 <code>AGENTS.md</code> 为准，
接口细节以 <code>docs/bklv-api.md</code> / <code>docs/json-syntax.md</code> 为准，本页是它们的合并速查版。</footer>
</main>
</div>
<button class="top" id="top" title="回到顶部">↑</button>
<script>
(function () {{
  var aside = document.querySelector('aside');

  // 目录过滤
  var search = document.getElementById('search');
  search.addEventListener('input', function () {{
    var q = search.value.trim().toLowerCase();
    document.querySelectorAll('.toc a').forEach(function (a) {{
      var hit = !q || a.textContent.toLowerCase().indexOf(q) >= 0;
      a.parentElement.style.display = hit ? '' : 'none';
    }});
    document.querySelectorAll('.toc > li').forEach(function (li) {{
      var kids = li.querySelectorAll('ul a');
      if (!kids.length) return;
      var any = Array.prototype.some.call(kids, function (a) {{ return a.parentElement.style.display !== 'none'; }});
      var self = li.querySelector(':scope > a');
      var selfHit = self && self.parentElement.style.display !== 'none';
      li.style.display = (any || selfHit || !q) ? '' : 'none';
      var ul = li.querySelector('ul');
      if (ul) ul.style.display = (any || !q) ? '' : 'none';
    }});
  }});

  // 滚动高亮
  var links = Array.prototype.slice.call(document.querySelectorAll('.toc a'));
  var targets = links.map(function (a) {{ return document.getElementById(a.getAttribute('href').slice(1)); }});
  function highlight() {{
    var y = window.scrollY + 120;
    var active = 0;
    for (var i = 0; i < targets.length; i++) {{
      if (targets[i] && targets[i].getBoundingClientRect().top + window.scrollY <= y) active = i;
    }}
    links.forEach(function (a, i) {{ a.classList.toggle('active', i === active); }});
    var btn = document.getElementById('top');
    btn.style.display = window.scrollY > 400 ? 'block' : 'none';
  }}
  window.addEventListener('scroll', highlight, {{ passive: true }});
  highlight();

  document.getElementById('top').addEventListener('click', function () {{
    window.scrollTo({{ top: 0, behavior: 'smooth' }});
  }});

  // 手机：点目录后收起列表，别挡住内容
  if (window.matchMedia('(max-width: 900px)').matches) {{
    links.forEach(function (a) {{
      a.addEventListener('click', function () {{ aside.scrollTop = 0; }});
    }});
  }}
}})();
</script>
</body>
</html>
'''

OUT.write_text(HTML, encoding='utf-8')
print(f'写好 {OUT}（{len(HTML) / 1024:.0f} KB，{len(toc_items)} 个标题）')
