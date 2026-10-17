#include <string>

#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/bklv.hpp"
#include "bklv/core/icon_font.hpp"

using namespace bklv;

namespace demo
{

// JSON 子页面：把「外观 JSON 的图层」和「视图过渡 transitions」两件事拆开演示。
//   ① 逐层：样例卡片有五层（shadow / bg / border / hlBg / icon / title），
//      每层单独展示一份「只有这一层可见」的变体外观（主题里用 import + 把其它层涂透明做的）
//   ② 合成：五层一起画出来的样子
//   ③ 动画：视图级 5 个通道 + 层内 when:"after" + 「什么都没写」的默认淡入淡出，
//      每条隔 1.6 秒重播一次自己的 enter（同一个 ViewTransition 单时间轴驱动）

namespace
{

// 样例外观与它的逐层变体都在主题里（base/json_demo*.json）
constexpr const char* kDemoAppearance = "base/json_demo";

// 小节标题（section）：照 borealis 的 Header —— 左边竖条 + 标题 + 底部 1px 线，
// 结构写在主题的 base/section.json 里（不是卡片）
Box* makeStepHeader(const char* titleKey)
{
    AppearanceView* section = new AppearanceView("base/section", false);
    section->setText(getStr(titleKey));
    return section;
}

// JSON 片段按列宽硬换行（Label 只在空格处折行，长片段不折会把整页撑宽）
std::string wrapSnippet(const std::string& text, size_t maxColumns = 34)
{
    std::string out;
    size_t lineLength = 0;
    size_t lastBreak  = std::string::npos;

    for (size_t i = 0; i < text.size(); i++)
    {
        const char c = text[i];
        if (c == '\n')
        {
            out += c;
            lineLength = 0;
            lastBreak  = std::string::npos;
            continue;
        }
        out += c;
        lineLength++;
        if (c == ' ' || c == ',')
            lastBreak = out.size();
        if (lineLength >= maxColumns)
        {
            if (lastBreak != std::string::npos)
            {
                out.insert(lastBreak, "\n");
                lineLength = out.size() - lastBreak - 1;
            }
            else
            {
                out += '\n';
                lineLength = 0;
            }
            lastBreak = std::string::npos;
        }
    }
    return out;
}

// 代码文本：小一号、次色、允许换行
Label* makeSnippet(const std::string& json)
{
    Label* label = makeText(wrapSnippet(json), getTheme().number("metric.font.size.sm", 15.0f),
        getTheme().color("color.text.dim"), false);
    label->setLineHeight(1.25f);
    return label;
}

// 一个小板块：标题一行 + 下面左右两块
//   左 = 渲染结果（JSON 解析出来的样子）  右 = 那段 JSON 代码
//
// 注意：样例外观自己写了 `width: 100%`，所以**两边都必须用固定比例的容器包住** ——
// 直接把卡片塞进横向行里的话，它会吃掉整行宽度、把代码列挤没，还会溢出盖住上面的说明
//（第一版就是这么坏的，截图才发现）。
Box* makeSplitRow(const char* titleKey, const std::string& json, View* demo)
{
    Box* row = makeCard();
    row->addView(makeText(getStr(titleKey), getTheme().number("metric.font.size.md", 18.0f),
        getTheme().color("color.text.primary")));

    Box* split = demo::makeRow(getTheme().number("metric.spacing.lg", 20.0f));
    split->setAlignItems(AlignItems::FLEX_START);

    // 左：渲染结果
    Box* left = makeColumn(0.0f);
    left->setWidthPercentage(52.0f);
    left->addView(demo);
    split->addView(left);

    // 右：代码块（底色 + 内边距，和左边的实物区分开）
    Box* code = makeColumn(0.0f);
    code->setWidthPercentage(46.0f);
    code->setPadding(getTheme().number("metric.spacing.md", 14.0f));
    code->setCornerRadius(getTheme().number("metric.radius.lg", 12.0f));
    code->setBackgroundColor(getTheme().color("color.surface.sunken"));
    code->addView(makeSnippet(json));
    split->addView(code);

    row->addView(split);
    return row;
}

// 一张演示卡：外观自己决定哪一层可见；图标 + 文字是内容槽
AppearanceView* makeDemoCard(const std::string& appearanceName)
{
    AppearanceView* card = new AppearanceView(appearanceName, false);
    card->setIcon(icon::glyphByName("photo_camera"));
    card->setText(getStr("json/card_text"));
    return card;
}

// 一张卡片，但外观来自**内联 JSON**（不是主题里的某一份）。
//
// 扩展层的演示用它：默认主题**不该**依赖 demo 注册的 op —— 所以这个层只出现在这一页，
// 不进 resources/theme/default（`base/stripe_demo.json` 那种做法会带来"库的随包资源依赖
// demo 代码"的问题，评审 §4.2 P0④ 记过）。
class InlineAppearanceCard : public AppearanceView
{
  public:
    InlineAppearanceCard(std::string appearanceJson)
        : AppearanceView("base/json_demo", false) // 兜底外观：内联那份编译失败也不至于空白
        , appearanceJson(std::move(appearanceJson))
    {
        this->applyInline();
    }

    // 换肤时**重新编译内联 JSON**（用新主题解析 @theme/ 引用）。
    // 不重写的话 applyTheme() 只会按名字取兜底外观，切完主题这张卡片就"变回"普通卡片了。
    void applyTheme() override
    {
        AppearanceView::applyTheme();
        this->applyInline();
    }

  private:
    void applyInline()
    {
        std::string error;
        if (!this->applyAppearanceJson(this->appearanceJson, error))
            BKLV_LOG_WARNING("内联外观编译失败（先用兜底外观显示）：%s", error.c_str());
    }

    std::string appearanceJson;
};

AppearanceView* makeInlineCard(const std::string& appearanceJson)
{
    InlineAppearanceCard* card = new InlineAppearanceCard(appearanceJson);
    card->setIcon(icon::glyphByName("category"));
    card->setText(getStr("json/card_text"));
    return card;
}

// 会自己重播 enter 的卡片：步进同一个 ViewTransition（改的都是绘制期变换 → 零重排）
class AnimDemo : public AppearanceView
{
  public:
    explicit AnimDemo(const std::string& appearanceName)
        : AppearanceView(appearanceName, false)
    {
        this->setIcon(icon::glyphByName("star"));
        this->setText(getStr("json/card_text"));
    }

    void update() override
    {
        AppearanceView::update(); // 约定：先递归子树，再干自己的事

        const float dt = getFrameDeltaSeconds();
        this->transition.step(dt);
        this->elapsed += dt;

        // 播完停一会儿再重播，方便盯着看每条通道单独在动什么
        if (!this->transition.isActive() && this->elapsed >= kIntervalSeconds)
        {
            this->elapsed = 0.0f;
            this->transition.playEnter(this, this->getWidth(), &this->getActiveAppearance());
        }
    }

  private:
    static constexpr float kIntervalSeconds = 1.6f;

    ViewTransition transition;
    float elapsed = kIntervalSeconds; // 第一帧就播
};

} // namespace

Box* buildJsonPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.sm", 8.0f));

    page->addView(makeHint(getStr("json/demo_intro")));

    // ---- ① 逐层单独展示 ----
    page->addView(makeStepHeader("json/layers_title"));
    page->addView(makeHint(getStr("json/layers_hint")));

    static const struct
    {
        const char* titleKey;
        const char* variant;
        const char* json;
    } kLayers[] = {
        { "json/layer_shadow", "shadow",
            R"({ "op": "shadow", "id": "shadow", "fill": "@theme/color.common.shadow",
  "feather": 16, "offset": [0, 4] })" },
        { "json/layer_bg", "bg",
            R"({ "op": "rect", "id": "bg", "fill": "@theme/color.surface.card", "radius": 14 })" },
        { "json/layer_border", "border",
            R"(// 描边和填充都在 bg 层上，分开写就能单独看
{ "op": "rect", "id": "bg", "stroke": "@theme/color.accent.base", "strokeWidth": 2 })" },
        { "json/layer_hlbg", "hlbg",
            R"({ "op": "rect", "id": "hlBg", "fill": "@theme/color.highlight.background" })" },
        { "json/layer_icon", "icon",
            R"({ "op": "text", "id": "icon", "source": "icon", "width": 36,
  "fontSize": 28, "color": "@theme/color.accent.base" })" },
        { "json/layer_title", "title",
            R"({ "op": "text", "id": "title", "source": "text", "inset": [0, 0, 0, 46],
  "fontSize": 20, "color": "@theme/color.text.primary" })" },
    };

    for (const auto& layer : kLayers)
    {
        const std::string name = std::string(kDemoAppearance) + "." + layer.variant;
        page->addView(makeSplitRow(layer.titleKey, layer.json, makeDemoCard(name)));
    }

    // ---- ② 合成效果 ----
    page->addView(makeStepHeader("json/composite_title"));
    page->addView(makeHint(getStr("json/composite_hint")));
    page->addView(makeSplitRow("json/composite_row",
        R"(// 逐层变体就是「import 这份 + 把别的层涂透明」
{ "import": "base/json_demo" })",
        makeDemoCard(kDemoAppearance)));

    // 第三方扩展层：`op` 是 demo/contrib/stripe_layer.cpp 注册的（bklv/ 里没有这个类型）。
    // 这条是"可扩展性"的实物：加层不用改库，也不用改这一页。
    //
    // 外观**内联在这一页**，不放 resources/theme/default —— 库的随包资源不该依赖
    // 由调用方注册的 op（评审 §4.2 P0④；`base/stripe_demo.json` 就是这么撤掉的）。
    const std::string stripeJson = R"({
  "box": { "width": "100%", "height": 64 },
  "layers": [
    { "op": "rect", "id": "bg", "fill": "@theme/color.surface.card", "radius": 12 },
    { "op": "stripe", "id": "stripes", "angle": 45, "lineWidth": 3, "gap": 16,
      "fill": "@theme/color.accent.base", "radius": 12, "inset": [6, 6, 6, 6] }
  ]
})";
    page->addView(makeSplitRow("json/extension_row", stripeJson, makeInlineCard(stripeJson)));

    // 第二个扩展层：四个顶点各自随机游走（`op` 由 demo/contrib/quad_layer.cpp 注册）。
    // 注意这一行的 JSON 里有 **7 个**自定义参数 —— 它们不需要库认识，解析/缩放/校验
    // 全由注册时声明的参数模式负责。
    page->addView(makeSplitRow("json/quad_row",
        R"({ "op": "quad", "amplitude": 10, "speed": 1.2, "seed": 7,
  "gain0": 1.0, "gain1": 0.6, "gain2": 1.3, "gain3": 0.8,
  "fill": "@theme/color.accent.base", "stroke": "@theme/color.text.primary",
  "strokeWidth": 2, "inset": 6 })",
        makeInlineCard(R"({ "box": { "width": 160, "height": 90 },
  "layers": [
    { "op": "rect", "id": "bg", "fill": "@theme/color.surface.card", "radius": 10 },
    { "op": "quad", "id": "blob", "amplitude": 10, "speed": 1.2, "seed": 7,
      "gain0": 1.0, "gain1": 0.6, "gain2": 1.3, "gain3": 0.8,
      "fill": "@theme/color.accent.base", "stroke": "@theme/color.text.primary",
      "strokeWidth": 2, "inset": 6 }
  ] })")));

    // children：**一份外观 = 一个完整控件**。base/children_demo 自己声明了子节点
    // （图标块 + 一列"标题/副标题"），页面这边只是把它当普通外观用，没有一行 C++ 描述结构。
    page->addView(makeSplitRow("json/children_row",
        R"({ "box": { "axis": "row", "gap": 10, "padding": 12 },
  "children": [
    { "appearance": "…", "box": { "width": 36, "height": 36 } },
    { "box": { "grow": 1 }, "children": [ … ] } ] })",
        makeDemoCard("base/children_demo")));

    // ---- ③ 动画逐个演示 ----
    page->addView(makeStepHeader("json/anim_title"));
    page->addView(makeHint(getStr("json/anim_hint")));

    static const struct
    {
        const char* titleKey;
        const char* variant;
        const char* json;
    } kAnims[] = {
        { "json/anim_translatex", "anim.translatex",
            R"("enter": { "duration": 420, "easing": "quadraticOut",
           "translateX": { "from": -120, "to": 0 } })" },
        { "json/anim_translatey", "anim.translatey",
            R"("enter": { "duration": 420, "easing": "quadraticOut",
           "translateY": { "from": -70, "to": 0 } })" },
        { "json/anim_scalex", "anim.scalex",
            R"("enter": { "duration": 420, "easing": "quadraticOut",
           "scaleX": { "from": 0.55, "to": 1 } })" },
        { "json/anim_scaley", "anim.scaley",
            R"("enter": { "duration": 420, "easing": "quadraticOut",
           "scaleY": { "from": 0.55, "to": 1 } })" },
        { "json/anim_alpha", "anim.alpha",
            R"("enter": { "duration": 420, "easing": "quadraticOut",
           "alpha": { "from": 0, "to": 1 } })" },
        { "json/anim_after", "anim.after",
            R"("enter": { "duration": 260,
           "alpha": { "from": 0, "to": 1 },
           "layers": { "title": { "when": "after", "duration": 260,
                                  "offsetY": { "from": 18, "to": 0 },
                                  "opacity": { "from": 0, "to": 1 } } } })" },
    };

    for (const auto& anim : kAnims)
    {
        const std::string name = std::string(kDemoAppearance) + "." + anim.variant;
        page->addView(makeSplitRow(anim.titleKey, anim.json, new AnimDemo(name)));
    }

    // 「什么都没写」的样子：默认淡入淡出
    page->addView(makeSplitRow("json/anim_default",
        R"(// 外观里不写 transitions —— 默认就是淡入淡出
// （时长取主题 metric.anim.normal）
{ "import": "base/json_demo" })",
        new AnimDemo(kDemoAppearance)));

    return page;
}

} // namespace demo
