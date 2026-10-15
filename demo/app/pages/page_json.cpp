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

// 步骤 header：一张卡片 + 强调色标题
Rectangle* makeStepHeader(const char* titleKey)
{
    Rectangle* card = makeCard();
    card->addView(makeText(getStr(titleKey), getTheme().number("metric.font.size.md", 18.0f),
        getTheme().color("color.accent.base", 0)));
    return card;
}

// JSON 片段按列宽硬换行（Label 只在空格处折行，长片段不折会把整页撑宽）
std::string wrapSnippet(const std::string& text, size_t maxColumns = 46)
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

// 一行：说明文字 + JSON 片段 + 演示卡
Box* makeRow(const char* titleKey, const std::string& json, View* demo)
{
    Box* row = makeCard();
    row->addView(makeText(getStr(titleKey), getTheme().number("metric.font.size.md", 18.0f),
        getTheme().color("color.text.primary")));
    row->addView(makeSnippet(json));
    row->addView(demo);
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
        page->addView(makeRow(layer.titleKey, layer.json, makeDemoCard(name)));
    }

    // ---- ② 合成效果 ----
    page->addView(makeStepHeader("json/composite_title"));
    page->addView(makeHint(getStr("json/composite_hint")));
    page->addView(makeRow("json/composite_row",
        R"(// 逐层变体就是「import 这份 + 把别的层涂透明」
{ "import": "base/json_demo" })",
        makeDemoCard(kDemoAppearance)));

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
        page->addView(makeRow(anim.titleKey, anim.json, new AnimDemo(name)));
    }

    // 「什么都没写」的样子：默认淡入淡出
    page->addView(makeRow("json/anim_default",
        R"(// 外观里不写 transitions —— 默认就是淡入淡出
// （时长取主题 metric.anim.normal）
{ "import": "base/json_demo" })",
        new AnimDemo(kDemoAppearance)));

    return page;
}

} // namespace demo
