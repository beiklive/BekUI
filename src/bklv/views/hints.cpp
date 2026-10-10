#include "bklv/views/hints.hpp"

#include <vector>

#include "bklv/core/font.hpp"
#include "bklv/core/input.hpp"

namespace bklv
{

void drawHints(ImDrawList* drawList, float screenWidth, float screenHeight)
{
    const std::vector<HintItem> hints = getInputManager().getHints();
    if (hints.empty())
        return;

    ImFont* font = getDefaultFont();
    if (font == nullptr)
        return;

    constexpr float fontSize   = 15.0f;
    constexpr float keyBoxW    = 26.0f;
    constexpr float keyBoxH    = 20.0f;
    constexpr float gapKeyText = 6.0f;
    constexpr float gapItems   = 22.0f;
    constexpr float paddingX   = 16.0f;
    constexpr float paddingY   = 8.0f;
    constexpr float barHeight  = keyBoxH + paddingY * 2.0f;

    // 先量出整条提示栏的宽度
    float contentWidth = 0.0f;
    std::vector<float> textWidths;
    textWidths.reserve(hints.size());

    for (const HintItem& hint : hints)
    {
        const float textWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, hint.hintText.c_str()).x;
        textWidths.push_back(textWidth);
        contentWidth += keyBoxW + gapKeyText + textWidth + gapItems;
    }
    contentWidth -= gapItems;

    const float barWidth = contentWidth + paddingX * 2.0f;
    const float barX     = (screenWidth - barWidth) * 0.5f;
    const float barY     = screenHeight - barHeight - 14.0f;

    drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barWidth, barY + barHeight),
        IM_COL32(20, 22, 27, 210), barHeight * 0.5f);

    float cursor = barX + paddingX;

    for (size_t i = 0; i < hints.size(); ++i)
    {
        const float keyY = barY + paddingY;

        // 按键小方块
        drawList->AddRectFilled(ImVec2(cursor, keyY), ImVec2(cursor + keyBoxW, keyY + keyBoxH),
            IM_COL32(70, 78, 92, 255), 4.0f);

        const char* label = getControllerButtonName(hints[i].button);
        const ImVec2 labelSize = font->CalcTextSizeA(fontSize - 2.0f, FLT_MAX, 0.0f, label);
        drawList->AddText(font, fontSize - 2.0f,
            ImVec2(cursor + (keyBoxW - labelSize.x) * 0.5f, keyY + (keyBoxH - labelSize.y) * 0.5f),
            IM_COL32(240, 243, 248, 255), label);

        cursor += keyBoxW + gapKeyText;

        // 提示文字
        const float textY = keyY + (keyBoxH - fontSize) * 0.5f - 1.0f;
        drawList->AddText(font, fontSize, ImVec2(cursor, textY), IM_COL32(190, 198, 210, 255),
            hints[i].hintText.c_str());

        cursor += textWidths[i] + gapItems;
    }
}

} // namespace bklv
