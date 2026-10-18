#include "app/card_button.hpp"

#include <cmath>

#include "bklv/core/input.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/time.hpp"
#include "bklv/core/logger.hpp"

using namespace bklv;

namespace demo
{

namespace
{
int g_cardIndex = 0;
}

CardButton::CardButton(const std::string& title, const std::string& subtitle,
    const std::string& icon)
    : AppearanceView("base/button/card", true)
{
    this->phase = 0.7f * (float)g_cardIndex++;
    this->onPointerAction = true;
    this->setText(title);
    this->setInfoText(subtitle);
    this->setIcon(icon);
    this->registerAction(ControllerButton::A, "打开", [this](View*) {
        if (this->onClick)
            this->onClick();
    });
}

void CardButton::onFocusGained()
{
    AppearanceView::onFocusGained();
    this->focused = true;
}

void CardButton::onFocusLost()
{
    AppearanceView::onFocusLost();
    this->focused = false;
}

void CardButton::update()
{
    AppearanceView::update();

    const float dt = std::clamp(getFrameDeltaSeconds(), 0.0f, 0.1f);
    this->time += dt;

    // 目标：聚焦时抬起 + 轻微倾斜，平时只做很小的呼吸浮动
    const float targetLift = this->focused ? -16.0f : 0.0f;
    const float bob        = std::sin(this->time * 1.6f + this->phase) * 3.0f;
    const float targetTilt = this->focused
        ? std::sin(this->time * 1.2f + this->phase) * 0.012f - 0.02f
        : bob * 0.0008f;

    // 指数逼近：不和帧率绑定，变速也不会"跳"
    const float k = 1.0f - std::exp(-12.0f * dt);
    this->lift += (targetLift - this->lift) * k;
    this->tilt += (targetTilt - this->tilt) * k;

    this->setTranslateY(this->lift + bob);
    this->setRotate(this->tilt);
    const float scale = 1.0f + (this->focused ? 0.04f : 0.0f);
    this->setScaleX(scale);
    this->setScaleY(scale);
}

} // namespace demo
