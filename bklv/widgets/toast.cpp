#include "bklv/widgets/toast.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/core/icon_font.hpp"
#include "bklv/core/time.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "bklv/widgets/content_ids.hpp"

using namespace bklv;

namespace
{

const char* appearanceFor(ToastType type)
{
    switch (type)
    {
        case ToastType::SUCCESS: return "base/toast/toast.success";
        case ToastType::ERROR:   return "base/toast/toast.error";
        case ToastType::INFO:
        default:                 return "base/toast/toast.info";
    }
}

const char* iconFor(ToastType type)
{
    switch (type)
    {
        case ToastType::SUCCESS: return "check";
        case ToastType::ERROR:   return "error";
        case ToastType::INFO:
        default:                 return "info";
    }
}

float easeOutCubic(float t)
{
    const float u = 1.0f - std::clamp(t, 0.0f, 1.0f);
    return 1.0f - u * u * u;
}

} // namespace

ToastManager::ToastManager()
    : Box(Axis::COLUMN)
{
    this->setPositionType(PositionType::ABSOLUTE);
    this->setPositionTop(20.0f);
    this->setPositionRight(8.0f);
    this->setWidth(320.0f);
    this->setGap(10.0f);
    this->setAlignItems(AlignItems::FLEX_END);
    this->setClipsToBounds(false);
}

void ToastManager::show(ToastType type, const std::string& message)
{
    if (message.empty())
        return;

    AppearanceView* view = new AppearanceView(appearanceFor(type), false);
    view->setText(content::TEXT, message);
    view->setIcon(content::ICON, icon::glyphByName(iconFor(type)));
    view->setTranslateX(360.0f);
    view->setAlpha(0.0f);
    this->addView(view);

    Entry entry;
    entry.view = view;
    this->entries.push_back(entry);
}

void ToastManager::clear()
{
    for (Entry& entry : this->entries)
        if (entry.view != nullptr)
            this->removeView(entry.view);
    this->entries.clear();
}

void ToastManager::update()
{
    Box::update();

    const float dt = getFrameDeltaSeconds();
    if (dt <= 0.0f)
        return;

    for (size_t i = 0; i < this->entries.size();)
    {
        Entry& entry = this->entries[i];
        if (entry.view == nullptr)
        {
            this->entries.erase(this->entries.begin() + (long)i);
            continue;
        }

        if (!entry.exiting)
        {
            // 入场：240ms 内从 0 → 1，用 easeOutCubic
            entry.slide = std::min(1.0f, entry.slide + dt / std::max(this->enterDuration, 0.001f));

            if (entry.slide >= 1.0f)
            {
                entry.elapsed += dt;
                if (entry.elapsed >= this->visibleDuration)
                    entry.exiting = true;
            }
        }
        else
        {
            entry.exitAt += dt;
            entry.slide = std::max(0.0f,
                1.0f - entry.exitAt / std::max(this->exitDuration, 0.001f));

            if (entry.slide <= 0.0f)
            {
                AppearanceView* view = entry.view;
                this->entries.erase(this->entries.begin() + (long)i);
                this->removeView(view); // removeView(view, true) 会 delete
                continue;
            }
        }

        const float eased = easeOutCubic(entry.slide);
        entry.view->setTranslateX((1.0f - eased) * 340.0f);
        entry.view->setAlpha(eased);
        i++;
    }
}
