#include "bklv/widgets/focus_ring_view.hpp"

#include "bklv/core/image_decoder.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/time.hpp"

namespace bklv
{

namespace
{

// 色谱贴图（GBAStation 那张 512x4 的 1D LUT，青 ↔ 紫两个循环）。
constexpr const char* kLutPath = "resources/focus_ring_gradient.png";

} // namespace

FocusRingView::FocusRingView()
{
    // 只负责画：铺满浮层（绝对定位四边 0）、不吃输入。
    // 可聚焦性走 View::isFocusable() 的默认实现（false），这里不需要额外关。
    this->setPositionType(PositionType::ABSOLUTE);
    this->setPositionTop(0.0f);
    this->setPositionRight(0.0f);
    this->setPositionBottom(0.0f);
    this->setPositionLeft(0.0f);
}

FocusRingView::~FocusRingView()
{
    if (this->lut.isValid())
        releaseTexture(this->lut);
}

void FocusRingView::ensureTexture()
{
    if (this->lut.isValid() || this->textureLoadAttempted)
        return;

    // 视图树常常在 App::run() 之前就建好了，那时图形后端还传不了纹理 —— 之后每帧重试。
    if (!isTextureStoreReady())
        return;

    this->textureLoadAttempted = true;

    const DecodedImage image = decodeImageFile(resourcePath(kLutPath).c_str());
    if (!image.isValid())
    {
        Logger::warning(std::string("焦点框色谱贴图加载失败：") + kLutPath
            + (image.getError().empty() ? std::string() : "（" + image.getError() + "）"));
        return;
    }

    this->lut = loadTextureFromPixels(image.pixels.data(), image.width, image.height);
    if (!this->lut.isValid())
        Logger::warning(std::string("焦点框色谱贴图上传失败：") + kLutPath);
}

bool FocusRingView::isRingVisible() const
{
    if (!this->ringEnabled || this->style.type == focus_ring::Type::NONE || !this->lut.isValid())
        return false;

    const View* focus = getInputManager().getCurrentFocus();
    if (focus == nullptr || !focus->isFocusable())
        return false;

    const focus_ring::Outline outline = focus->getFocusOutline();
    return outline.bounds.width > 0.0f && outline.bounds.height > 0.0f;
}

void FocusRingView::stepPhase(float deltaSeconds)
{
    this->elapsed += deltaSeconds;
}

void FocusRingView::update()
{
    this->ensureTexture();

    // 没焦点就不推进时钟：静态界面下这里每帧只是两次判断。
    if (this->isRingVisible())
        this->stepPhase(getFrameDeltaSeconds());
}

void FocusRingView::draw(
    ImDrawList* drawList, float x, float y, float width, float height, FrameContext* ctx)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)ctx;

    if (!this->ringEnabled || this->style.type == focus_ring::Type::NONE)
        return;

    const View* focus = getInputManager().getCurrentFocus();
    if (focus == nullptr || !focus->isFocusable())
        return;

    // 形状由控件自己回答；圆角够大时它自己就是圆/胶囊，这里不需要判断形状种类。
    const focus_ring::Outline outline = focus->getFocusOutline();
    if (outline.bounds.width <= 0.0f || outline.bounds.height <= 0.0f)
        return;

    // 贴图由本视图持有，复制一份样式再填进去 —— 外部通过 getRingStyle() 改参数时
    // 不会把纹理句柄一起改坏。
    focus_ring::Style effective = this->style;
    effective.lut               = this->lut;
    effective.alpha *= focus->getAlpha();

    // 相位由流动时钟算出来：flowSeconds <= 0 时恒为 0（等于不流动）
    const float phase = focus_ring::phaseForTime(this->elapsed, effective.flowSeconds);
    focus_ring::draw(drawList, outline, effective, phase);
}

} // namespace bklv
