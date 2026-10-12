#include "app/pages.hpp"

#include "app/ui.hpp"

#include <cmath>
#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

namespace
{

constexpr float PI = 3.14159265f;

// 舞台：里面那个方块是"被变换的对象"，四个补间分别驱动它的一组属性。
// 对标 borealis demo 的 TransformTab / TransformBox——区别是他们靠在自定义视图的
// draw 里手写 nvg 变换，我们用的是 View 上的绘制期变换（任何控件都能用）。
class TransformStage : public Box
{
  public:
    TransformStage()
        : Box(Axis::COLUMN)
    {
        this->setWidthPercentage(100.0f);
        this->setHeight(getTheme().number("metric.transform.stageHeight", 300.0f));
        this->setAlignItems(AlignItems::CENTER);
        this->setJustifyContent(JustifyContent::CENTER);

        // 被变换的方块：底 + 图 + 文字（文字用 fontScale 单独缩放）
        this->subject = new Rectangle("card");
        this->subject->setWidth(160.0f);
        this->subject->setHeight(160.0f);

        Box* column = new Box(Axis::COLUMN);
        column->setGrow(1.0f);
        column->setAlignItems(AlignItems::CENTER);
        column->setJustifyContent(JustifyContent::CENTER);
        column->setGap(getTheme().number("metric.spacing.sm", 8.0f));

        Image* icon = new Image();
        icon->setWidth(72.0f);
        icon->setHeight(72.0f);
        icon->setImageFromFile("resources/testcard.png");
        column->addView(icon);

        this->caption = new Label();
        this->caption->setText("bklv");
        this->caption->setFontSize(getTheme().number("metric.transform.captionSize", 20.0f));
        column->addView(this->caption);

        this->subject->addView(column);
        this->addView(this->subject);
    }

    void setSubjectSize(float size)
    {
        this->subject->setWidth(size);
        this->subject->setHeight(size);
    }

    Rectangle* getSubject() const { return this->subject; }
    Label* getCaption() const { return this->caption; }

    void applyTransform(float transX, float transY, float scaleX, float scaleY, float skewX,
        float skewY, float rotate, float fontScaleX, float fontScaleY)
    {
        // 布局还没算出来时 getWidth/getHeight 会是 NaN：这里兜底成 0，
        // 免得 NaN 传进变换（库那边也会拦，但应用侧不该制造 NaN）
        auto finite = [](float value) { return std::isfinite(value) ? value : 0.0f; };

        const float stage   = finite(this->getWidth());
        const float stageH  = finite(this->getHeight());
        Rectangle* box      = this->subject;
        const float boxW    = finite(box->getWidth());
        const float boxH    = finite(box->getHeight());

        box->setTranslateX((stage - boxW) * 0.5f * transX);
        box->setTranslateY((stageH - boxH) * 0.5f * transY);
        box->setScaleX(scaleX);
        box->setScaleY(scaleY);
        box->setSkewX(skewX * PI);
        box->setSkewY(skewY * PI);
        box->setRotate(rotate * PI * 2.0f);

        this->caption->setFontScaleX(fontScaleX);
        this->caption->setFontScaleY(fontScaleY);
    }

    void resetAll()
    {
        Animatable* tweens[4] = { &this->aniX, &this->aniY, &this->skew, &this->skew2 };
        for (Animatable* tween : tweens)
            tween->stop();

        this->applyTransform(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    // borealis 的 Play：四段多步补间 + 不同缓动曲线
    void play()
    {
        (void)PI;

        this->aniX.stop();
        this->aniY.stop();
        this->skew.stop();
        this->skew2.stop();

        this->aniX.reset(0.0f);
        this->aniX.addStep(1.0f, 1.6f, EasingFunction::exponentialOut);
        this->aniX.addStep(0.0f, 1.6f, EasingFunction::cubicIn);
        this->aniX.addStep(0.5f, 1.6f, EasingFunction::exponentialOut);
        this->aniX.addStep(0.0f, 1.6f, EasingFunction::cubicIn);

        this->aniY.reset(0.0f);
        this->aniY.addStep(1.0f, 1.6f, EasingFunction::bounceOut);
        this->aniY.addStep(0.0f, 1.6f, EasingFunction::cubicIn);
        this->aniY.addStep(1.0f, 1.6f, EasingFunction::bounceOut);
        this->aniY.addStep(0.0f, 1.6f, EasingFunction::cubicIn);

        this->skew.reset(0.0f);
        this->skew.addStep(1.0f, 1.6f, EasingFunction::bounceOut);
        this->skew.addStep(0.0f, 1.6f, EasingFunction::cubicIn);

        this->skew2.reset(0.0f);
        this->skew2.addStep(0.0f, 1.6f);
        this->skew2.addStep(1.0f, 1.6f, EasingFunction::bounceOut);
        this->skew2.addStep(0.0f, 1.6f, EasingFunction::cubicIn);

        this->aniX.start();
        this->aniY.start();
        this->skew.start();
        this->skew2.start();
    }

    // 手动值（拖动滑块时用；会停掉补间）
    void setManual(float transX, float transY, float scaleX, float scaleY, float skewX, float skewY,
        float rotate, float fontScaleX, float fontScaleY)
    {
        this->aniX.stop();
        this->aniY.stop();
        this->skew.stop();
        this->skew2.stop();
        this->applyTransform(transX, transY, scaleX, scaleY, skewX, skewY, rotate, fontScaleX,
            fontScaleY);
    }

    void update() override
    {
        Box::update();



        if (this->aniX.isStopped() && this->aniY.isStopped() && this->skew.isStopped()
            && this->skew2.isStopped())
            return;

        // 补间在跑：把它们当前的值写到方块上（滑块也同步，方便看数值）
        const float transX = (float)this->aniX;
        const float transY = (float)this->aniY;
        const float skewX  = (float)this->skew;
        const float skewY  = (float)this->skew2;

        this->applyTransform(transX, transY, 1.0f - skewX * 0.4f, 1.0f - skewY * 0.4f, skewX, skewY,
            0.0f, 1.0f + skewX * 0.5f, 1.0f);

        if (this->onTweenTick)
            this->onTweenTick(transX, transY, skewX, skewY);
    }

    std::function<void(float, float, float, float)> onTweenTick;
    std::function<void()> onResetSliders;
    std::function<void()> onValuesChanged;

  private:
    Rectangle* subject = nullptr;
    Label* caption     = nullptr;

    Animatable aniX{ 0.0f };
    Animatable aniY{ 0.0f };
    Animatable skew{ 0.0f };
    Animatable skew2{ 0.0f };
};

} // namespace

Box* buildTransformPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("transform/hint")));

    TransformStage* stage = new TransformStage();
    Rectangle* stageCard  = makeCard();
    stageCard->addView(stage);
    page->addView(stageCard);

    // ---- 播放 / 复位 ----
    Box* controls = makeRow(getTheme().number("metric.spacing.sm", 8.0f));
    Button* play  = new Button(getStr("transform/play"));
    play->setOnClick([stage] { stage->play(); });
    controls->addView(play);

    Button* reset = new Button(getStr("transform/reset"));
    reset->setOnClick([stage] {
        stage->resetAll();
        if (stage->onResetSliders)
            stage->onResetSliders();
    });
    controls->addView(reset);
    page->addView(controls);

    // ---- 滑块（SliderCell 既是控制也是演示）----
    Rectangle* sliders = makeCard();
    sliders->addView(makeText(getStr("transform/sliders_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    struct Row
    {
        SliderCell* cell;
        float* value;
    };

    float transX = 0.0f, transY = 0.0f, scaleX = 1.0f, scaleY = 1.0f;
    float skewX = 0.0f, skewY = 0.0f, rotate = 0.0f, fontSX = 1.0f, fontSY = 1.0f;

    auto addSlider = [&](const char* key, float minValue, float maxValue, float initial,
                         float* slot) {
        SliderCell* cell = new SliderCell(getStr(key));
        cell->setRange(minValue, maxValue);
        cell->setValue(initial);
        *slot = initial;
        cell->setOnChange([&, slot](float value) {
            *slot = value;
            stage->setManual(transX, transY, scaleX, scaleY, skewX, skewY, rotate, fontSX, fontSY);
            if (stage->onValuesChanged)
                stage->onValuesChanged();
        });
        sliders->addView(cell);
        return cell;
    };

    SliderCell* cellTransX = addSlider("transform/trans_x", -1.0f, 1.0f, 0.0f, &transX);
    SliderCell* cellTransY = addSlider("transform/trans_y", -1.0f, 1.0f, 0.0f, &transY);
    SliderCell* cellScaleX = addSlider("transform/scale_x", 0.0f, 2.0f, 1.0f, &scaleX);
    SliderCell* cellScaleY = addSlider("transform/scale_y", 0.0f, 2.0f, 1.0f, &scaleY);
    SliderCell* cellSkewX  = addSlider("transform/skew_x", -0.5f, 0.5f, 0.0f, &skewX);
    SliderCell* cellSkewY  = addSlider("transform/skew_y", -0.5f, 0.5f, 0.0f, &skewY);
    SliderCell* cellRotate = addSlider("transform/rotate", 0.0f, 1.0f, 0.0f, &rotate);
    SliderCell* cellFontX  = addSlider("transform/font_x", 0.5f, 2.0f, 1.0f, &fontSX);
    SliderCell* cellFontY  = addSlider("transform/font_y", 0.5f, 2.0f, 1.0f, &fontSY);
    page->addView(sliders);

    // 补间跑的时候把滑块也带着动（borealis 的 setTickCallback 就是这个用途）
    stage->onTweenTick = [cellTransX, cellTransY, cellSkewX, cellSkewY](
                             float x, float y, float sx, float sy) {
        cellTransX->setValue(x);
        cellTransY->setValue(y);
        cellSkewX->setValue(sx);
        cellSkewY->setValue(sy);
    };

    stage->onResetSliders = [cellTransX, cellTransY, cellScaleX, cellScaleY, cellSkewX, cellSkewY,
                                cellRotate, cellFontX, cellFontY] {
        cellTransX->setValue(0.0f);
        cellTransY->setValue(0.0f);
        cellScaleX->setValue(1.0f);
        cellScaleY->setValue(1.0f);
        cellSkewX->setValue(0.0f);
        cellSkewY->setValue(0.0f);
        cellRotate->setValue(0.0f);
        cellFontX->setValue(1.0f);
        cellFontY->setValue(1.0f);
    };

    return page;
}

} // namespace demo
