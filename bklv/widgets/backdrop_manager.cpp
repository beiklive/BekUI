#include "bklv/widgets/backdrop_manager.hpp"

#include "bklv/core/logger.hpp"

namespace bklv
{

void BackdropManager::add(const std::string& pageId, View* backdrop)
{
    if (backdrop == nullptr)
        return;

    this->backdrops[pageId] = backdrop;
    if (this->hostLayer != nullptr)
        this->hostLayer->addView(backdrop);
    backdrop->setVisibility(Visibility::GONE);

    // 第一份背景直接当当前背景
    if (this->current == nullptr)
    {
        this->current   = backdrop;
        this->currentId = pageId;
        backdrop->setVisibility(Visibility::VISIBLE);
        backdrop->setAlpha(1.0f);
    }
}

View* BackdropManager::get(const std::string& pageId) const
{
    auto it = this->backdrops.find(pageId);
    return it == this->backdrops.end() ? nullptr : it->second;
}

void BackdropManager::show(const std::string& pageId)
{
    if (pageId == this->currentId)
        return;

    auto it = this->backdrops.find(pageId);
    if (it == this->backdrops.end() || it->second == nullptr)
    {
        BKLV_LOG_WARNING("BackdropManager::show: 页面 %s 没有注册背景", pageId.c_str());
        return;
    }

    View* next = it->second;

    // 新的移到宿主层末尾：子节点顺序 = 绘制顺序，淡入的那份要画在旧背景之上
    if (this->hostLayer != nullptr && next->getParent() == this->hostLayer)
        this->hostLayer->removeView(next, false);
    if (this->hostLayer != nullptr)
        this->hostLayer->addView(next);

    // 旧的先留着垫在下面，补间结束再 GONE（否则淡入期间会露出画布底色）
    this->previous = this->current;
    if (this->previous != nullptr)
        this->previous->setAlpha(1.0f);

    next->setVisibility(Visibility::VISIBLE);
    next->setAlpha(0.0f);

    this->current   = next;
    this->currentId = pageId;

    if (this->duration <= 0.0f)
    {
        next->setAlpha(1.0f);
        if (this->previous != nullptr)
            this->previous->setVisibility(Visibility::GONE);
        this->previous = nullptr;
        return;
    }

    this->anim.stop();
    this->anim.reset(0.0f);
    this->anim.addStep(1.0f, this->duration, EasingFunction::CUBIC_OUT);
    this->anim.start();
}

void BackdropManager::step(float deltaSeconds)
{
    if (this->anim.isStopped())
        return;

    this->anim.update(deltaSeconds);

    const float alpha = (float)this->anim;
    if (this->current != nullptr)
        this->current->setAlpha(alpha);

    if (alpha >= 1.0f && this->previous != nullptr)
    {
        this->previous->setVisibility(Visibility::GONE);
        this->previous = nullptr;
    }
}

} // namespace bklv
