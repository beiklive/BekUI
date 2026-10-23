// JSON 主题能力：演示控件全部来自 resources/theme/default/demo/ 下的外观 JSON，
// 右侧滚动显示这些 JSON 的原文（改 JSON 就能改样式，不用改 C++）。

#include "app/pages.hpp"
#include "app/pages/showcase.hpp"

#include "bklv/core/icon_font.hpp"
#include "bklv/core/time.hpp"
#include "bklv/core/transition.hpp"
#include "bklv/core/json_view.hpp"

using namespace bklv;

namespace
{

AppearanceView* makeDemoView(const char* appearance, const std::string& title,
    const std::string& info, const char* iconName)
{
    AppearanceView* view = new AppearanceView(appearance, false);
    view->setText(content::TEXT, title);
    if (!info.empty())
        view->setText(content::INFO, info);
    if (iconName != nullptr)
        view->setIcon(content::ICON, bklv::icon::glyphByName(iconName));
    return view;
}

// states 演示：没有可聚焦控件，靠定时切换状态，避免打开页面就被自动滚到某个按钮。
class StateDemo : public Box
{
  public:
    StateDemo()
        : Box(Axis::COLUMN)
    {
        this->setAlignItems(AlignItems::CENTER);
        this->setJustifyContent(JustifyContent::CENTER);
        this->view = new AppearanceView("demo/state_button", false);
        this->view->setText(content::TEXT, "状态按钮");
        this->addView(this->view);
        this->apply();
    }

    void update() override
    {
        Box::update();
        this->elapsed += getFrameDeltaSeconds();
        if (this->elapsed >= 0.9f)
        {
            this->elapsed = 0.0f;
            this->phase   = (this->phase + 1) % 3;
            this->apply();
        }
    }

    std::string getClassString() const override { return "demo::StateDemo"; }

  private:
    void apply()
    {
        this->view->setAppearanceState("focused", this->phase == 0);
        this->view->setAppearanceState("pressed", this->phase == 1);
    }

    AppearanceView* view = nullptr;
    float elapsed = 0.0f;
    int phase = 0; // 0 focused / 1 pressed / 2 常态
};

// transitions 需要一个持有 ViewTransition 的对象逐帧 step；
// 用一个小容器包住「面板 + 播放按钮」，点按钮在 show / hide 之间切换。
class TransitionDemo : public Box
{
  public:
    TransitionDemo()
        : Box(Axis::COLUMN)
    {
        this->setAlignItems(AlignItems::CENTER);
        this->setJustifyContent(JustifyContent::CENTER);
        this->setGap(12.0f);

        Box* stage = new Box(Axis::COLUMN);
        stage->setWidth(320.0f);
        stage->setHeight(150.0f);
        stage->setAlignItems(AlignItems::CENTER);
        stage->setJustifyContent(JustifyContent::CENTER);

        this->panel = new AppearanceView("demo/transition_card", false);
        this->panel->setText(content::TEXT, "过渡面板");
        this->panel->setText(content::INFO, "show / hide");
        stage->addView(this->panel);
        this->addView(stage);

        this->transition.playShow(this->panel);
    }

    void update() override
    {
        Box::update();
        this->transition.step(getFrameDeltaSeconds());

        // 自动循环：show 停 1.6s → hide → 停 1.6s → show
        this->elapsed += getFrameDeltaSeconds();
        if (this->elapsed >= 1.6f)
        {
            this->elapsed = 0.0f;
            if (this->shown)
            {
                this->transition.playHide(this->panel,
                    [this] { this->panel->setVisibility(Visibility::GONE); });
            }
            else
            {
                this->panel->setVisibility(Visibility::VISIBLE);
                this->transition.playShow(this->panel);
            }
            this->shown = !this->shown;
        }
    }

    std::string getClassString() const override { return "demo::TransitionDemo"; }

  private:
    ViewTransition transition;
    AppearanceView* panel = nullptr;
    bool shown = true;
    float elapsed = 0.0f;
};

} // namespace

namespace demo
{

Box* buildJsonThemePage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setGap(getTheme().number("metric.spacing.lg", 24.0f));

    page->addView(makeShowcaseSection("JSON · 渐变 + 阴影 + 多文本层",
        makeDemoView("demo/gradient_card", "主题卡片", "gradient + shadow", "star"),
        readThemeJson("demo/gradient_card.json"),
        "一个外观里叠了 shadow / 渐变 bg / 三个 text 层；右侧 JSON 改颜色或 inset，界面立刻变。"));

    page->addView(makeShowcaseSection("JSON · box 段：尺寸 / 内边距 / 白底 / 圆角 / 边框",
        makeDemoView("demo/box_card", "box 样式", "padding + border + radius", nullptr),
        readThemeJson("demo/box_card.json"),
        "不写任何背景层：尺寸、内边距、底色、圆角、边框全部由 box 段描述。文本层再用 align/valign 定位。"));

    page->addView(makeShowcaseSection("JSON · 图片层（SVG src）+ 圆形底 + 标题",
        makeDemoView("demo/badge", "徽章", "", nullptr),
        readThemeJson("demo/badge.json"),
        "image 层的 src 相对主题根（demo/img/star.svg），支持 SVG/PNG/JPG；同文件里还有圆底、阴影和标题层。"));

    page->addView(makeShowcaseSection("JSON · 胶囊 + 描边 + 阴影",
        makeDemoView("demo/pill", "开始游戏", "", nullptr),
        readThemeJson("demo/pill.json"),
        "胶囊 = 圆角取高度的一半；描边、阴影、文字对齐都只改 JSON。"));

    page->addView(makeShowcaseSection("JSON · 内阴影 + 描边 + 图标 / 信息",
        makeDemoView("demo/inner_card", "内阴影卡片", "inner: true", "settings"),
        readThemeJson("demo/inner_card.json"),
        "同一个 shadow op，加 inner: true 就变成内阴影（凹陷感）；图标层用 font_kind: icon。"));

    page->addView(makeShowcaseSection("JSON · states：聚焦 / 按下（自动循环演示）",
        new StateDemo(),
        readThemeJson("demo/state_button.json"),
        "0.9s 切换一次：focused 换底色和字色，pressed 用 self.translateY 下移并换按下色；"
        "补间时长/缓动都写在 states 里。"));

    page->addView(makeShowcaseSection("JSON · import：复用基础外观 + 覆盖 + 追加",
        makeDemoView("demo/import_child", "import 子外观", "override + append", nullptr),
        readThemeJson("demo/import_child.json"),
        "子外观 import 了 demo/import_base：继承它的 box 与 title 层，覆盖 bg.fill（渐变让位），"
        "再追加 info 层和 shadow bundle；父文件本身是独立可用的外观。"));

    page->addView(makeShowcaseSection("JSON · transitions：显示 / 隐藏动画（自动循环）",
        new TransitionDemo(),
        readThemeJson("demo/transition_card.json"),
        "外观里的 transitions 描述 show / hide 的 alpha 与 translateY；这里每 1.6s 自动播一次，"
        "hide 播完才真正隐藏。"));

    // 页面级 JSON：整个页面由 view/page_json.json 描述
    {
        Box* stage = new Box(Axis::COLUMN);
        stage->setWidthPercentage(100.0f);
        stage->setAlignItems(AlignItems::CENTER);

        View* jsonPage = createViewFromTheme("page_json");
        if (jsonPage != nullptr)
        {
            jsonPage->setWidth(440.0f);
            stage->addView(jsonPage);
        }

        page->addView(makeShowcaseSection("JSON · 页面级描述（view/page_json.json）", stage,
            readThemeJson("view/page_json.json"),
            "控件结构也能由 JSON 描述：createViewFromTheme 建出整棵视图树，C++ 只负责挂上去。"));
    }

    // 主题切换：default / glass（glass 会打开外壳玻璃）
    {
        Box* row = new Box(Axis::ROW);
        row->setGap(12.0f);
        row->setAlignItems(AlignItems::CENTER);

        Button* defaultTheme = new Button("默认主题");
        defaultTheme->setOnClick([] { applyTheme("default"); });
        row->addView(defaultTheme);

        Button* glassTheme = new Button("玻璃主题");
        glassTheme->setOnClick([] { applyTheme("glass"); });
        row->addView(glassTheme);

        page->addView(makeShowcaseSection("主题 · 切换 default / glass", row,
            readThemeJson("theme.json"),
            "glass 基于 default 派生：只覆盖上下栏与弹层的玻璃层，其余缺项回退 default；"
            "切换时同时开关外壳玻璃。"));
    }

    // JSON 行为：onClick 名字 -> C++ 注册
    {
        Box* stage = new Box(Axis::COLUMN);
        stage->setWidthPercentage(100.0f);
        stage->setAlignItems(AlignItems::CENTER);

        if (View* actionView = createViewFromTheme("json_action"))
            stage->addView(actionView);

        page->addView(makeShowcaseSection("JSON · 行为段（onClick）", stage,
            readThemeJson("view/json_action.json"),
            "结构归 JSON，行为归 C++：onClick 只写名字，"
            "registerJsonAction(\"json.demoToast\", ...) 在宿主里实现。"));
    }

    return page;
}

} // namespace demo
