# JSON 视图描述：设计笔记

移植 borealis 的 XML 视图系统，但按项目约定改用 JSON。这份笔记记录**为什么这么设计**，
以及哪些地方是有意偏离 borealis 的。

## 1. 与 borealis 的对应关系

| 能力 | borealis（XML + tinyxml2） | bklv（JSON + nlohmann/json） |
|---|---|---|
| 描述格式 | `<brls:Box width="auto">` | `{ "type": "Box", "width": "auto" }` |
| 类型注册 | `Application::registerXMLView(name, creator)` | `registerViewType(name, creator)` |
| 属性值 | 字符串 + 前缀语义 | 字符串前缀语义 **+ 原生 JSON 数字/布尔** |
| 单个属性归属 | 每个 View 实例挂 7 个 `unordered_map` + `set<string>` | **每个类一张 static 表**，handler 是函数指针 |
| 复用 | `<brls:View xml="...">` | `{ "include": "resources/views/x.json" }` |
| 文档缓存 | `getXMLCache()`：按 `std::hash<std::string>` 缓存，无上限 | 按**完整路径**缓存，32 条 LRU，带失败负缓存 |
| 出错 | 未知标签 `fatal()` 崩进程；属性错只打印名字与值 | 结构错误返回 `nullptr` + 行号/路径；未知属性警告 + 拼错提示 |
| 数据绑定 | 靠子类化 `Box` + `inflateFromXMLElement` | `"bind": "字段名"` + app 侧 binder 回调 |

## 2. 为什么必须两段式

```
ViewTemplate（解析一次、可缓存、与语言/主题无关）
      ↓ instantiate(ctx)
视图树（每次实例化新建；@i18n/ @style/ @theme/ 在这一步解析）
```

模板缓存存在的唯一理由就是"同一份描述实例化很多次"——回收列表里每个 cell 都是同一份模板。

如果解析期就把 `@i18n/` 替换成具体文案，缓存里就烤死了当时的语言：
界面已经是英文了，cell 还在显示中文。这一条是设计里最容易搞错的地方，
测试 `tests/json_test.cpp` 第 8 节专门守着它（同一模板切语言后重实例化必须换语言）。

## 3. 属性系统

属性表按类静态（`View::attributes()` 虚函数返回类级 static 表），handler 是普通函数指针：

```cpp
const AttributeTable& Label::attributes() const
{
    static const AttributeTable table = {
        { "text", AttributeKind::Text, setTextAttr, nullptr },
        { "fontSize", AttributeKind::NUMBER, setFontSizeAttr, nullptr },
        // ...
    };
    return table;
}
```

`AttributeKind` 决定值怎么转换、出错时怎么措辞：

| kind | 接受 | 说明 |
|---|---|---|
| `Number` | `12`、`"12"`、`"12px"`、`"@style/a/b"` | 尾缀 px 会被剥掉 |
| `Size` | `Number` 的形式 + `"auto"`（→ `AUTO`/NAN）+ `"50%"` | 百分比走 `setXxxPercentage` |
| `Bool` | `true` / `"true"` | |
| `Color` | `"#RRGGBB"`、`"#RRGGBBAA"`、`"@theme/name"` | |
| `Text` | 字符串，`@i18n/key` 会翻译 | |
| `String` | 字符串，不做任何解析 | |
| `ResourcePath` | `"@res/x"`，**相对 `resources/` 目录** | 与 borealis 的 `BRLS_RESOURCES` 语义一致 |
| `Enum` | 必须落在 `allowed`（`"row|column"`）里 | 大小写不敏感，回传标准写法 |

不认识但值不对的属性会给出人话：「期望 尺寸（数字 / "auto" / "50%"），得到 "abc"」；
属性名拼错会给编辑距离最近的候选。这是相对 borealis（静默丢弃）最主要的手感差异。

## 4. 缓存

```cpp
std::unordered_map<std::string, CacheEntry> // key = 完整路径，不是 hash
std::list<std::string>                      // LRU 顺序，超过 32 条淘汰
```

borealis 的 `getXMLCache` 用 `std::hash<std::string>(path)` 当 key：碰撞会**静默返回别人的文档**，
而且缓存无上限、失败也不区分。这里三处都改了：
- key 用完整路径（碰撞不可能）
- 32 条 LRU（动态构造的路径不会把内存吃光）
- 失败也缓存（负缓存）——不然每帧都会重读一次盘、刷一遍日志

## 5. 数据绑定

RecyclerFrame 的 cell 是复用的：`setIndex(i)` 换行号，同一个 cell 要显示第 i 行的数据。
所以"JSON 定义的列表项"必须回答"第 i 行显示什么"。

做法是让 JSON 只声明**字段名**，取值交给 app 的 binder：

```json
{ "type": "Image", "bind": "thumb", "width": 76, "height": 44 }
{ "type": "Label", "bind": "title" }
{ "type": "Box",   "bind": "accent", "bindTo": "backgroundColor" }
```

```cpp
list->registerCell(RecyclerDataSource::DEFAULT_CELL_ID, [tmpl] {
    return JSONRecyclerCell::create(tmpl, [](const std::string& field, size_t index) {
        if (field == "title") return titles[index];
        return std::string();
    });
});
```

- `bind` 的默认目标属性按类型定：`Label` → `text`，`Image` → `src`，其他类型必须写 `bindTo`
  （写不清楚就不猜，直接报错要求显式指定）
- 绑定在实例化时收集（`ViewTemplate::instantiate(&bindings)`），include 进来的也收
- 取值失败只警告，不让整页崩

这样 JSON 不需要知道数据结构，`RecyclerDataSource` 也一行不用改。

## 6. 上限与错误模型

| 项 | 值 | 原因 |
|---|---|---|
| 嵌套深度 | 32 层 | 畸形/恶意描述不该把 Switch 拖死 |
| 节点总数 | 4096 | 同上 |
| include 栈深 | 32 层 | 被引入的模板从自己的深度 0 开始算，光靠深度拦不住长 include 链 |
| 循环 include | 检测并报错 | `a -> b -> a` 会一路递归到爆栈（**实现时踩到过**） |

结构错误 → `nullptr` + 日志（含 `文件:行号` 或 `json_pointer` 路径）；
未知属性 → 警告，继续建树。语法错误的行号来自 nlohmann 的 `parse_error.byte` 折算。

实例化全程用 `std::unique_ptr<View>` 持有，中途失败/抛出不会漏掉已建好的子树。

## 7. 有意不做的

- **不引 tinyxml2、不做 XML 兼容层**：项目约定不用 XML
- **不扫目录**列模板：与 i18n 同样的理由，romfs 上的目录迭代真机未验证
- **RecyclerFrame 不注册为 JSON 类型**：它必须先有数据源才有意义，光有类型会建出一个空列表
- **热重载未做**：Switch 上资源在只读 romfs 里，桌面能做但本轮不做
- **P4（脏标记布局、属性表进一步静态化）单独一轮**：收益最大但改动面最广，
  和 JSON 移植混做会让两类故障互相掩盖

## 8. 相关文件

```
bklv/ui/theme/attributes.hpp/.cpp   属性值与属性表
bklv/ui/view/json_view.hpp/.cpp    模板、缓存、注册表、实例化、错误
bklv/ui/widgets/list/json_cell.hpp/.cpp   JSON 模板驱动的列表项（绑定 + 焦点视觉）
resources/views/*.json          示例描述（page_json.json / list_item.json）
tests/json_test.cpp             80 项断言：往返 / 尺寸 / 畸形 / 绑定 / 缓存 / 语言
```
