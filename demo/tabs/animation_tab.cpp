#include "animation_tab.hpp"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>

#include "bklv/foundation/anim/time.hpp"
#include "bklv/foundation/anim/transition.hpp"
#include "bklv/system/media/icon_font.hpp"
#include "bklv/ui/input/gesture.hpp"
#include "bklv/ui/theme/appearance.hpp"
#include "bklv/ui/theme/theme_data.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/appearance/appearance_view.hpp"
#include "bklv/ui/widgets/basic/image.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/container/scroll_box.hpp"
#include "bklv/ui/widgets/controls/button.hpp"
#include "demo/demo_app.hpp"
#include "demo/layout_kit.hpp"
#include "demo/style.hpp"

namespace demo
{

namespace
{

constexpr float kCardW    = 240.0f;
constexpr float kCardH    = 260.0f;
constexpr float kFrame    = 404.0f;
constexpr float kStepSecs = 1.2f;

// ---------------------------------------------------------------------------
// 动画演示用的滑条（项目里没有 Slider 控件，这里只看指针拖动，不抢焦点）。
// 一行 = 标题 + 轨道（filled + knob）+ 数值；拖动整行都能改值。
// ---------------------------------------------------------------------------
class DemoSlider : public bklv::Box
{
  public:
    DemoSlider(const std::string& title, float initial, std::function<void(float)> onChange,
        std::function<void(bool)> onDragState = {})
        : Box(bklv::Axis::ROW)
        , callback(std::move(onChange))
        , dragState(std::move(onDragState))
    {
        this->setGrow(1.0f);
        this->setHeight(28.0f);
        this->setGap(8.0f);
        this->setAlignItems(bklv::AlignItems::CENTER);
        // 整行能接住指针（可点 ≠ 可聚焦），拖动轨道才有效
        this->setPointerTarget(true);

        this->titleLabel = makeNote(title);
        this->titleLabel->setWidth(96.0f);
        this->titleLabel->setFontSize(15.0f);
        this->addView(this->titleLabel);

        this->track = new bklv::Box(bklv::Axis::ROW);
        this->track->setGrow(1.0f);
        this->track->setHeight(18.0f);
        this->track->setCornerRadius(4.0f);
        this->track->setBackgroundColor(
            bklv::getTheme().color("color.surface.sunken", IM_COL32(224, 227, 232, 255)));
        this->track->setClipsToBounds(true);

        this->fill = new bklv::Box(bklv::Axis::ROW);
        this->fill->setPositionType(bklv::PositionType::ABSOLUTE);
        this->fill->setPositionLeft(0.0f);
        this->fill->setPositionTop(0.0f);
        this->fill->setHeightPercentage(100.0f);
        this->fill->setCornerRadius(4.0f);
        this->fill->setBackgroundColor(
            bklv::getTheme().color("color.accent.base", IM_COL32(25, 118, 210, 255)));
        this->track->addView(this->fill);

        this->knob = new bklv::Box(bklv::Axis::ROW);
        this->knob->setPositionType(bklv::PositionType::ABSOLUTE);
        this->knob->setWidth(16.0f);
        this->knob->setHeight(16.0f);
        this->knob->setPositionTop(1.0f);
        this->knob->setCornerRadius(8.0f);
        this->knob->setBackgroundColor(
            bklv::getTheme().color("color.common.white", IM_COL32(255, 255, 255, 255)));
        this->track->addView(this->knob);

        this->addView(this->track);

        this->valueLabel = makeNote("");
        this->valueLabel->setWidth(54.0f);
        this->valueLabel->setFontSize(14.0f);
        this->addView(this->valueLabel);

        auto* pan = new bklv::PanGestureRecognizer({
            [this](const bklv::Point& position) {
                if (this->dragState)
                    this->dragState(true);
                this->setValueFromPointer(position.x);
            },
            [this](const bklv::Point& delta) { this->addFromPointer(delta.x); },
            [this]() {
                if (this->dragState)
                    this->dragState(false);
            },
        });
        this->addGesture(std::unique_ptr<bklv::GestureRecognizer>(pan));

        this->setValue(initial, false);
    }

    std::string getClassString() const override { return "demo::DemoSlider"; }

    void setValue(float value, bool notify = true)
    {
        this->value = std::clamp(value, 0.0f, 1.0f);
        char text[16];
        std::snprintf(text, sizeof(text), "%.2f", this->value);
        if (this->valueLabel != nullptr)
            this->valueLabel->setText(text);
        if (notify && this->callback)
            this->callback(this->value);
    }

    float getValue() const { return this->value; }

    void update() override
    {
        Box::update();
        if (this->track == nullptr || this->fill == nullptr || this->knob == nullptr)
            return;
        const float width = this->track->getWidth();
        if (width <= 0.0f)
            return;
        this->fill->setWidth(std::max(1.0f, width * this->value));
        this->knob->setPositionLeft(std::clamp(
            width * this->value - this->knob->getWidth() * 0.5f, 0.0f, width - this->knob->getWidth()));
    }

  private:
    void setValueFromPointer(float x)
    {
        const float width = this->track->getWidth();
        if (width <= 0.0f)
            return;
        this->setValue((x - this->track->getX()) / width);
    }

    void addFromPointer(float dx)
    {
        const float width = this->track->getWidth();
        if (width <= 0.0f)
            return;
        this->setValue(this->value + dx / width);
    }

    bklv::Label* titleLabel = nullptr;
    bklv::Label* valueLabel = nullptr;
    bklv::Box* track        = nullptr;
    bklv::Box* fill         = nullptr;
    bklv::Box* knob         = nullptr;
    std::function<void(float)> callback;
    std::function<void(bool)> dragState;
    float value = 0.0f;
};

// ---------------------------------------------------------------------------
// Transform 动画：参考 borealis demo 的 transform_tab
//  - 左侧一排滑条（transX/Y、scaleX/Y、skewX/Y、rotate、width/height、fontScaleX/Y）
//  - 右侧 404x404 预览框里 100x100 的图，Play 用 Animatable 连续改这些通道
// ---------------------------------------------------------------------------
class TransformDemo : public bklv::AppearanceView
{
  public:
    explicit TransformDemo(bklv::ScrollBox* page)
        : AppearanceView("base/animation/demo_row", false)
        , page(page)
    {
        this->build();
        this->resetAll();
    }

    std::string getClassString() const override { return "demo::TransformDemo"; }

    void update() override
    {
        AppearanceView::update();
        if (!this->playing)
            return;

        const float dt = bklv::getFrameDeltaSeconds();
        this->aniX.update(dt);
        this->aniY.update(dt);
        this->skew.update(dt);
        this->skew2.update(dt);

        // 和参考 demo 一样：滑条跟着动画走，tick 里把 scale 与 skew 配对
        this->sliders[0]->setValue(this->aniX.getValue());
        this->sliders[1]->setValue(this->aniY.getValue());
        this->sliders[5]->setValue(this->skew.getValue());          // skewY
        this->sliders[2]->setValue(1.0f - this->skew.getValue());   // scaleX
        this->sliders[4]->setValue(this->skew2.getValue());         // skewX
        this->sliders[3]->setValue(1.0f - this->skew2.getValue());  // scaleY

        if (this->aniX.isStopped() && this->aniY.isStopped() && this->skew.isStopped()
            && this->skew2.isStopped())
            this->playing = false;
    }

  private:
    void build()
    {
        // ---- 左：播放/重置 + 11 个滑条 ----
        auto* left = new bklv::Box(bklv::Axis::COLUMN);
        left->setGrow(1.0f);
        left->setFlexBasis(0.0f);
        left->setShrink(1.0f);
        left->setGap(6.0f);

        auto* buttons = new bklv::Box(bklv::Axis::ROW);
        buttons->setGap(10.0f);
        buttons->setHeight(38.0f);

        auto* play = new bklv::TextButton("播放");
        play->setGrow(1.0f);
        play->setOnClick([this] { this->play(); });
        buttons->addView(play);

        auto* reset = new bklv::TextButton("重置");
        reset->setGrow(1.0f);
        reset->setOnClick([this] { this->resetAll(); });
        buttons->addView(reset);
        left->addView(buttons);

        auto* previewFrame = new bklv::AppearanceView("base/animation/preview_frame", false);
        previewFrame->setClipsToBounds(false);
        this->preview = new bklv::AppearanceView("base/animation/card_transform", false);
        this->preview->setWidth(kCardW);
        this->preview->setHeight(kCardH);
        // 玻璃框按 layout 矩形合成；rotate/skew/scale 只作用在内容上，
        // 裁到卡片里，内容不会跑到玻璃框外面。
        this->preview->setClipsToBounds(true);

        auto* image = new bklv::Image();
        image->setImageFromFile("resources/testcard.png");
        image->setScalingType(bklv::ImageScalingType::FILL);
        image->setWidthPercentage(100.0f);
        image->setHeight(150.0f);
        image->setCornerRadius(5.0f);
        this->preview->addView(image);

        auto* infoRow = new bklv::Box(bklv::Axis::ROW);
        infoRow->setGap(8.0f);
        infoRow->setAlignItems(bklv::AlignItems::CENTER);
        auto* icon = new bklv::Label();
        icon->setIconFontEnabled(true);
        icon->setFontSize(22.0f);
        icon->setText(bklv::icon::glyph(bklv::icon::Id::PLAY_ARROW));
        icon->setTextColor(bklv::getTheme().color("color.accent.base", IM_COL32(25, 118, 210, 255)));
        infoRow->addView(icon);
        this->demoLabel = makeParagraph("变换卡片");
        infoRow->addView(this->demoLabel);
        this->preview->addView(infoRow);
        previewFrame->addView(this->preview);

        // 11 个滑条：初值按参考 demo（trans/skew/rotate 0，scale/width/height/font 1）
        auto* panel = new bklv::Box(bklv::Axis::COLUMN);
        panel->setPadding(12.0f);
        panel->setGap(8.0f);
        panel->setCornerRadius(5.0f);
        panel->setBackgroundColor(
            bklv::getTheme().color("color.surface.raised", IM_COL32(236, 238, 242, 255)));
        left->addView(panel);

        auto add = [this, panel](const char* title, float initial, std::function<void(float)> cb) {
            auto* slider = new DemoSlider(title, initial, std::move(cb),
                [this](bool dragging) {
                    if (this->page != nullptr)
                        this->page->setScrollingEnabled(!dragging);
                });
            this->sliders.push_back(slider);
            panel->addView(slider);
        };
        this->leftColumn = left;

        add("transX", 0.0f,
            [this](float v) { this->preview->setTranslationX(v * (kFrame - kCardW)); });
        add("transY", 0.0f,
            [this](float v) { this->preview->setTranslationY(v * (kFrame - kCardH)); });
        add("scaleX", 1.0f, [this](float v) { this->preview->setScaleX(v * 2.0f - 1.0f); });
        add("scaleY", 1.0f, [this](float v) { this->preview->setScaleY(v * 2.0f - 1.0f); });
        add("skewX", 0.0f, [this](float v) { this->preview->setSkewX(v * 3.1415926f); });
        add("skewY", 0.0f, [this](float v) { this->preview->setSkewY(v * 3.1415926f); });
        add("rotate", 0.0f, [this](float v) { this->preview->setRotate(v * 6.2831853f); });
        add("width", 1.0f, [this](float v) { this->preview->setWidth(kCardW * (0.5f + 0.5f * v)); });
        add("height", 1.0f, [this](float v) { this->preview->setHeight(kCardH * (0.5f + 0.5f * v)); });
        add("fontSX", 1.0f, [this](float v) { this->demoLabel->setFontScaleX(v * 2.0f - 1.0f); });
        add("fontSY", 1.0f, [this](float v) { this->demoLabel->setFontScaleY(v * 2.0f - 1.0f); });

        // ---- 右：预览框 ----
        auto* right = new bklv::Box(bklv::Axis::COLUMN);
        right->setGrow(1.0f);
        right->setFlexBasis(0.0f);
        right->setShrink(1.0f);
        right->setAlignItems(bklv::AlignItems::CENTER);
        right->setJustifyContent(bklv::JustifyContent::CENTER);
        right->addView(previewFrame);

        this->addView(left);
        this->addView(right);
    }

    void play()
    {
        this->stopAll();
        this->playing = true;

        auto resetAnim = [](bklv::Animatable& anim) {
            anim.stop();
            anim.reset(0.0f);
        };
        resetAnim(this->aniX);
        this->aniX.addStep(1.0f, kStepSecs, bklv::EasingFunction::EXPONENTIAL_OUT);
        this->aniX.addStep(0.0f, kStepSecs, bklv::EasingFunction::CUBIC_IN);
        this->aniX.addStep(0.5f, kStepSecs, bklv::EasingFunction::EXPONENTIAL_OUT);
        this->aniX.addStep(0.0f, kStepSecs, bklv::EasingFunction::CUBIC_IN);

        resetAnim(this->aniY);
        this->aniY.addStep(1.0f, kStepSecs, bklv::EasingFunction::BOUNCE_OUT);
        this->aniY.addStep(0.0f, kStepSecs, bklv::EasingFunction::CUBIC_IN);
        this->aniY.addStep(1.0f, kStepSecs, bklv::EasingFunction::BOUNCE_OUT);
        this->aniY.addStep(0.0f, kStepSecs, bklv::EasingFunction::CUBIC_IN);

        resetAnim(this->skew);
        this->skew.addStep(1.0f, kStepSecs, bklv::EasingFunction::BOUNCE_OUT);
        this->skew.addStep(0.0f, kStepSecs, bklv::EasingFunction::CUBIC_IN);

        resetAnim(this->skew2);
        this->skew2.addStep(0.0f, kStepSecs * 2.0f, bklv::EasingFunction::LINEAR);
        this->skew2.addStep(1.0f, kStepSecs, bklv::EasingFunction::BOUNCE_OUT);
        this->skew2.addStep(0.0f, kStepSecs, bklv::EasingFunction::CUBIC_IN);

        this->aniX.start();
        this->aniY.start();
        this->skew.start();
        this->skew2.start();
    }

    void stopAll()
    {
        this->aniX.stop();
        this->aniY.stop();
        this->skew.stop();
        this->skew2.stop();
        this->playing = false;
    }

    void resetAll()
    {
        this->stopAll();
        const float values[] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        for (size_t i = 0; i < this->sliders.size() && i < sizeof(values) / sizeof(values[0]); i++)
            this->sliders[i]->setValue(values[i]);
    }

    bklv::AppearanceView* preview = nullptr;
    bklv::Label* demoLabel        = nullptr;
    bklv::Box* leftColumn         = nullptr;
    std::vector<DemoSlider*> sliders;
    bklv::Animatable aniX, aniY, skew, skew2;
    bklv::ScrollBox* page = nullptr;
    bool playing = false;
};

// JSON transition 示例：外观里写 transitions.show，按钮触发 playShow。
class TransitionCard : public bklv::AppearanceView
{
  public:
    TransitionCard()
        : AppearanceView("base/animation/card_transition", false)
    {
        this->buildContent();
    }

    std::string getClassString() const override { return "demo::TransitionCard"; }

    void play() { this->transition.playShow(this); }

    void update() override
    {
        AppearanceView::update();
        this->transition.step(bklv::getFrameDeltaSeconds());
    }

  private:
    void buildContent()
    {
        auto* image = new bklv::Image();
        image->setImageFromFile("resources/testcard.png");
        image->setScalingType(bklv::ImageScalingType::FILL);
        image->setWidthPercentage(100.0f);
        image->setHeight(150.0f);
        image->setCornerRadius(5.0f);
        this->addView(image);

        auto* row = new bklv::Box(bklv::Axis::ROW);
        row->setGap(8.0f);
        row->setAlignItems(bklv::AlignItems::CENTER);

        auto* icon = new bklv::Label();
        icon->setIconFontEnabled(true);
        icon->setFontSize(22.0f);
        icon->setText(bklv::icon::glyph(bklv::icon::Id::PLAY_ARROW));
        icon->setTextColor(bklv::getTheme().color("color.accent.base", IM_COL32(25, 118, 210, 255)));
        row->addView(icon);
        row->addView(makeParagraph("JSON 动画"));
        this->addView(row);
    }

    bklv::ViewTransition transition;
};

} // namespace

bklv::View* buildAnimationTab(DemoApp& app)
{
    (void)app;

    auto* scroll = new bklv::ScrollBox(bklv::Axis::COLUMN);
    scroll->setGrow(1.0f);

    auto* content = scroll->getContentBox();
    content->setGap(26.0f);
    content->setPadding(24.0f);

    // 1) 系统内部动画：参考 borealis transform_tab
    {
        auto* demo = new TransformDemo(scroll);
        content->addView(makeSection("变换动画", demo, "Play / Reset · trans / scale / skew / rotate / width / font"));
    }

    // 2) JSON 动画：transitions.show
    {
        auto* card = new TransitionCard();

        auto* play = new bklv::TextButton("播放 transition");
        play->setOnClick([card] { card->play(); });

        auto* row = new bklv::AppearanceView("base/animation/demo_row", false);

        auto* left = new bklv::Box(bklv::Axis::COLUMN);
        left->setGrow(1.0f);
        left->setFlexBasis(0.0f);
        left->setShrink(1.0f);
        left->setJustifyContent(bklv::JustifyContent::CENTER);
        left->setAlignItems(bklv::AlignItems::CENTER);
        left->addView(card);

        auto* right = new bklv::Box(bklv::Axis::COLUMN);
        right->setGrow(1.0f);
        right->setFlexBasis(0.0f);
        right->setShrink(1.0f);
        right->setGap(14.0f);
        right->setJustifyContent(bklv::JustifyContent::CENTER);
        right->setAlignItems(bklv::AlignItems::FLEX_START);
        auto* heading = makeParagraph("JSON transition");
        heading->setFontSize(22.0f);
        right->addView(heading);
        right->addView(makeNote("外观里写 transitions.show：\n"
                                "整卡 alpha 0→1，玻璃层 opacity 0.15→1。\n"
                                "480ms / quadraticOut，按钮每次重新播。"));
        right->addView(play);

        row->addView(left);
        row->addView(right);
        content->addView(makeSection("JSON 动画", row, "transitions.show · 淡入"));
    }

    return scroll;
}

} // namespace demo
