#include "bklv/widgets/appearance_view.hpp"

#include <algorithm>
#include <utility>
#include <cmath>

#include "bklv/core/font.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

AppearanceView::AppearanceView(std::string appearanceName, bool focusable)
    : appearanceName(std::move(appearanceName))
    , focusable(focusable)
{
}

void AppearanceView::setText(const std::string& value)
{
    if (this->text == value)
        return;

    this->text = value;
    this->invalidate();
}

void AppearanceView::setAppearanceVariant(const std::string& value)
{
    if (this->variant == value)
        return;

    this->variant = value;
    this->applyTheme();
}

void AppearanceView::applyTheme()
{
    // 换肤时重新取外观。AppearanceInstance 会抄新初值并清掉正在跑的补间，
    // 然后按当前活跃状态重新补——所以换肤过程中按钮不会卡在中间色。
    const std::string name = this->variant.empty() ? this->appearanceName
                                                   : this->appearanceName + "." + this->variant;
    this->appearance.setAppearance(getWidgetAppearance(name));

    if (this->appearance.getAppearance().getLayers().empty())
        BKLV_LOG_WARNING("主题里没有控件外观 \"%s\"（%s/widgets/%s.json）", name.c_str(),
            getTheme().getName().c_str(), name.c_str());

    // 状态在换肤后重新打开，让新外观按当前状态补到位
    if (this->isAppearanceStateActive("focused"))
        this->appearance.setState("focused", true);
}

void AppearanceView::setAppearanceState(const std::string& state, bool active)
{
    this->appearance.setState(state, active);
}

bool AppearanceView::isAppearanceStateActive(const std::string& state) const
{
    return this->appearance.isStateActive(state);
}

void AppearanceView::onFocusGained()
{
    this->setAppearanceState("focused", true);
}

void AppearanceView::onFocusLost()
{
    this->setAppearanceState("focused", false);
    this->setAppearanceState("pressed", false);
    this->pressed = false;
}

void AppearanceView::update()
{
    if (!this->appearance.isAnimating())
        return;

    // 补间在跑：用帧间隔推进（tweeny 内部按毫秒插值）
    this->appearance.update(ImGui::GetIO().DeltaTime);
}

std::string AppearanceView::getLayerText(const std::string& source)
{
    if (source == "text")
        return this->text;
    return {};
}

TextureRef AppearanceView::getLayerImage(const std::string&)
{
    return TextureRef();
}

void AppearanceView::layerRect(const AppearanceLayer& layer, float x, float y, float width,
    float height, float out[4]) const
{
    const float left   = x + layer.inset[3] + layer.offset[0];
    const float top    = y + layer.inset[0] + layer.offset[1];
    const float right  = x + width - layer.inset[1] + layer.offset[0];
    const float bottom = y + height - layer.inset[2] + layer.offset[1];

    out[0] = left;
    out[1] = top;
    out[2] = std::max(left, right);
    out[3] = std::max(top, bottom);
}

void AppearanceView::draw(ImDrawList* drawList, float x, float y, float width, float height, Style,
    FrameContext*)
{
    if (width <= 0.0f || height <= 0.0f)
        return;

    const Appearance& current = this->appearance.getAppearance();
    const std::vector<AppearanceLayer>& layers = current.getLayers();
    if (layers.empty())
        return;

    ImFont* font = getDefaultFont();

    for (size_t i = 0; i < layers.size(); i++)
    {
        const AppearanceLayer& layer = layers[i];
        const int index              = (int)i;

        const ImU32 fill        = this->appearance.getColor(index, AppearanceProp::Fill);
        const ImU32 stroke      = this->appearance.getColor(index, AppearanceProp::Stroke);
        const ImU32 textColor   = this->appearance.getColor(index, AppearanceProp::TextColor);
        const float strokeWidth = this->appearance.getNumber(index, AppearanceProp::StrokeWidth);
        const float radius      = this->appearance.getNumber(index, AppearanceProp::Radius);
        const float opacity     = this->appearance.getNumber(index, AppearanceProp::Opacity);
        const float fontSize    = this->appearance.getNumber(index, AppearanceProp::FontSize);

        float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        this->layerRect(layer, x, y, width, height, rect);

        // 逐层透明度：和控件自身的 alpha 相乘
        const float alphaScale = std::max(0.0f, std::min(1.0f, opacity)) * this->getAlpha();
        auto scaleAlpha        = [alphaScale](ImU32 color) {
            const unsigned int a = (color >> 24) & 0xFF;
            const unsigned int scaled = (unsigned int)std::round((float)a * alphaScale);
            return (color & 0x00FFFFFFu) | ((scaled & 0xFFu) << 24);
        };

        switch (layer.op)
        {
            case AppearanceLayer::Op::Rect:
            {
                if (alphaScale <= 0.001f)
                    break;
                if ((fill >> 24) != 0)
                    drawList->AddRectFilled(ImVec2(rect[0], rect[1]), ImVec2(rect[2], rect[3]),
                        scaleAlpha(fill), radius);
                if (strokeWidth > 0.0f && (stroke >> 24) != 0)
                    drawList->AddRect(ImVec2(rect[0], rect[1]), ImVec2(rect[2], rect[3]),
                        scaleAlpha(stroke), radius, 0, strokeWidth);
                break;
            }

            case AppearanceLayer::Op::Text:
            {
                const std::string content = this->getLayerText(layer.source);
                if (content.empty() || font == nullptr || alphaScale <= 0.001f)
                    break;

                const float size = fontSize > 0.0f ? fontSize : 15.0f;
                const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, content.c_str());

                float textX = rect[0];
                if (layer.align == "center")
                    textX = rect[0] + ((rect[2] - rect[0]) - textSize.x) * 0.5f;
                else if (layer.align == "right")
                    textX = rect[2] - textSize.x;

                float textY = rect[1];
                if (layer.valign == "center")
                    textY = rect[1] + ((rect[3] - rect[1]) - textSize.y) * 0.5f;
                else if (layer.valign == "bottom")
                    textY = rect[3] - textSize.y;

                drawList->AddText(font, size, ImVec2(textX, textY), scaleAlpha(textColor), content.c_str());
                break;
            }

            case AppearanceLayer::Op::Image:
            {
                const TextureRef texture = this->getLayerImage(layer.source);
                if (!texture.isValid() || alphaScale <= 0.001f)
                    break;

                const ImVec2 min(rect[0], rect[1]);
                const ImVec2 max(rect[2], rect[3]);
                const ImU32 tint = scaleAlpha(IM_COL32(255, 255, 255, 255));

                if (radius > 0.0f)
                    drawList->AddImageRounded(texture.texture, min, max, ImVec2(0, 0), ImVec2(1, 1), tint,
                        radius);
                else
                    drawList->AddImage(texture.texture, min, max, ImVec2(0, 0), ImVec2(1, 1), tint);
                break;
            }

            case AppearanceLayer::Op::Custom:
                this->drawCustomLayer(drawList, layer, rect[0], rect[1], rect[2] - rect[0],
                    rect[3] - rect[1], this->appearance);
                break;
        }
    }
}

} // namespace bklv
