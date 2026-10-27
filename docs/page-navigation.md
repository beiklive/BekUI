# 同级页面导航（PageRouter）

适用场景：

- 所有页面是**同一级**，没有父子关系；
- A 可以打开 C，A 可以打开 B，B 也可以打开 C；
- 启动程序可以直接进入 B；
- 从 B 可以返回 A。

当前框架**可以做到**，用 `Shell` 内容层的 `replaceContentView()`，再在宿主侧维护一个页面名历史。
不需要改 `ContentStack`。

## 1. 先分清 `push/pop` 和 `replace`

| 接口 | 语义 | 页面是否保留 | 返回 |
|---|---|---|---|
| `pushContentView(view, key)` | 历史栈压入一层 | **保留**（脱离树、在栈里） | `popContentView()` 弹回上一层 |
| `popContentView()` | 弹出栈顶并释放 | 栈顶删除 | 到上一层；`size <= 1` 时拒绝 |
| `replaceContentView(view, key)` | 同级换页 | 旧页出场后**释放** | **没有内置返回** |
| `fillLayer(view)` | 把页面设为绝对铺满内容层 | — | — |

`push/pop` 本质是**历史栈**，不是页面父子关系：B 里也可以 `push` C。
但被压住的页面不会销毁，游戏库这类重页面长期压着不划算；而且“直接启动 B、B 返回 A”需要一个 A 作为栈底。

所以“所有页面同级、只保留当前页、支持直接启动 B”用：

```text
replaceContentView + 宿主自己的 std::vector<std::string> history
```

## 2. PageRouter

```cpp
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

class PageRouter
{
  public:
    explicit PageRouter(bklv::Shell* shell)
        : shell(shell)
    {
    }

    void add(const std::string& name, std::function<bklv::View*()> factory)
    {
        this->pages[name] = std::move(factory);
    }

    // 启动：page 当前页；back 是预置的返回历史（栈底在 back.back()）
    void start(const std::string& page, std::vector<std::string> back = {})
    {
        this->history = std::move(back);
        this->show(page);
    }

    // 任意页面打开任意页面
    void open(const std::string& page)
    {
        if (page == this->current)
            return;

        if (!this->current.empty())
            this->history.push_back(this->current);

        this->show(page);
    }

    // B / 返回
    bool back()
    {
        if (this->history.empty())
            return false;

        const std::string previous = this->history.back();
        this->history.pop_back();
        this->show(previous);
        return true;
    }

    const std::string& getCurrent() const { return this->current; }

  private:
    void show(const std::string& page)
    {
        auto it = this->pages.find(page);
        if (it == this->pages.end())
            return;

        bklv::View* view = it->second();
        view->setId(page);
        this->shell->fillLayer(view);                  // 绝对铺满内容层
        this->shell->replaceContentView(view, page);   // 旧页出场后释放
        this->current = page;
    }

    bklv::Shell* shell = nullptr;
    std::map<std::string, std::function<bklv::View*()>> pages;
    std::vector<std::string> history;
    std::string current;
};
```

### 页面工厂

```cpp
bklv::View* makePageA(PageRouter& router)
{
    auto* page = new bklv::Box(bklv::Axis::COLUMN);
    page->setPadding(24.0f);
    page->setGap(16.0f);
    page->setBackgroundColor(bklv::getTheme().color(
        "color.surface.page", IM_COL32(242, 243, 245, 255)));

    auto* title = new bklv::Label();
    title->setText("A · 主页");
    title->setFontSize(30.0f);
    page->addView(title);

    auto* openB = new bklv::TextButton("打开 B");
    openB->setOnClick([&router] { router.open("B"); });
    page->addView(openB);

    auto* openC = new bklv::TextButton("打开 C");
    openC->setOnClick([&router] { router.open("C"); });
    page->addView(openC);

    return page;
}

bklv::View* makePageB(PageRouter& router)
{
    auto* page = new bklv::Box(bklv::Axis::COLUMN);
    page->setPadding(24.0f);
    page->setGap(16.0f);

    auto* title = new bklv::Label();
    title->setText("B · 设置");
    title->setFontSize(30.0f);
    page->addView(title);

    auto* openC = new bklv::TextButton("打开 C");
    openC->setOnClick([&router] { router.open("C"); });
    page->addView(openC);

    auto* backA = new bklv::TextButton("返回 A");
    backA->setOnClick([&router] { router.back(); });
    page->addView(backA);

    return page;
}

bklv::View* makePageC(PageRouter& router)
{
    auto* page = new bklv::Box(bklv::Axis::COLUMN);
    page->setPadding(24.0f);
    page->setGap(16.0f);

    auto* title = new bklv::Label();
    title->setText("C · 游戏库");
    title->setFontSize(30.0f);
    page->addView(title);

    auto* back = new bklv::TextButton("返回");
    back->setOnClick([&router] { router.back(); });
    page->addView(back);

    return page;
}
```

### 装配

```cpp
auto* shell = new bklv::Shell();
app.setRootView(shell);

PageRouter router(shell);
router.add("A", [&router] { return makePageA(router); });
router.add("B", [&router] { return makePageB(router); });
router.add("C", [&router] { return makePageC(router); });

shell->setEnterTransition(bklv::ContentTransition::SLIDE_FADE);
shell->setExitTransition(bklv::ContentTransition::SLIDE_FADE);
shell->setTransitionDuration(0.22f);

// 普通启动：A
// router.start("A");

// 直接启动到 B，但 B 的返回目标预置为 A
router.start("B", { "A" });

shell->setOnBackRequested([&router]() -> bool {
    return router.back();
});
```

## 3. 行为对照

| 操作 | history（页面名） | 当前页 | 返回一次 |
|---|---|---|---|
| `start("B", {"A"})` | `["A"]` | B | A |
| `start("A")` → `open("C")` | `["A"]` | C | A |
| `start("A")` → `open("B")` → `open("C")` | `["A","B"]` | C | B |
| 上一步再返回 | `["A"]` | B | A |
| 再返回 | `[]` | A | `back()` 返回 false，由宿主决定退出/忽略 |

- 所有页面同级：内容层里始终只有当前页，`getContentStackSize()` 恒为 1。
- 任意页面互相打开：`open()` 不检查父子关系，只记录当前页名字。
- 启动 B、返回 A：`start("B", {"A"})`，A 不预先构造，按返回时才构造。
- **注意**：B 如果是从 C 打开的，返回是 C（历史语义），不是固定 A。

## 4. 如果 B 永远返回 A

用固定返回表，不用 history：

```cpp
std::map<std::string, std::string> backTo = {
    { "B", "A" },
    { "C", "A" },
};

bool back()
{
    auto it = backTo.find(this->current);
    if (it == backTo.end())
        return false;

    this->show(it->second);
    return true;
}
```

这会把“返回”从历史语义改成“每页声明自己的返回目标”。
B 从 C 进入也会回 A；C 也回 A。按产品需要选一种。

## 5. 什么时候反而该用 push/pop

如果页面实例要**原样保留**（滚动位置、焦点、列表筛选状态、图片缓存），用 `pushContentView/popContentView` 更省事：

```cpp
shell->pushContentView(makePageA(), "A"); // 栈底
shell->pushContentView(makePageB(), "B"); // 当前 B
// B 里也可以 push C；pop 回 B，再 pop 回 A
```

特点：

- 被压住的页面不会销毁，状态自动保留；
- `pop` 自带焦点/滚动记忆（`ContentStack`）；
- 直接启动 B 需要先把 A 压进栈底，A 会参与一次入场/出场；
- 页面越压越多，重页面要注意内存。

一句话：

- **只保留当前页、同级路由、直接启动 B** → `replaceContentView` + `PageRouter`；
- **保留整页状态、自动回退** → `push/pop`。

## 6. 细节与坑

- **`replaceContentView` 会释放旧页**：不要保存旧页指针，不要在旧页回调里继续访问它。
- **`pushContentView` 接管所有权**：不要手动 `delete`；`pop` 时释放。
- **页面状态放 model**：`replace` 会重建页面；设置值、筛选条件、滚动位置如果要跨返回保留，放在 app/model 或改用 `push`。
- **`fillLayer` 后再 `replace`**：页面根用绝对定位铺满内容层；root 自己设 `padding`/背景。
- **转场是异步的**：`replace` 先播旧页出场再挂新页；连续快速点击可能叠导航，宿主可加 `navigating` 标志或在转场期间禁用按钮。
- **焦点**：`ContentStack::attach` 会给页面默认焦点；页面里至少有一个可聚焦控件（`TextButton` 等），否则焦点会留在外面。
- **B 的优先级**：`Dialog` / `Dropdown` 等模态先处理 B；模态关掉后才会到 `Shell::setOnBackRequested`。
- **页面根 id/key**：给页面 `setId("library")`，`replaceContentView(view, "library")` 用稳定名字；不要用指针地址当长期 key。
- **不要直接把主页 `addView` 到 `getContentLayer()` 再 `push` 子页**：`ContentStack` 里只有子页一层，`pop` 会拒绝。用本方案的 router，或把主页也作为栈底一层。

参考实现：

- `Shell::pushContentView` / `popContentView` / `replaceContentView` / `fillLayer`：`bklv/ui/shell/shell.hpp`
- `ContentStack` 的栈与转场：`bklv/ui/shell/content_stack.*`
- 层级下钻的现有 demo：`demo/demo_app.cpp` 的 `openListDetail` / `openGridDetail`（`push` 版本，不是本方案的同级 router）
