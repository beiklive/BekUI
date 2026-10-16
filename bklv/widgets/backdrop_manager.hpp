#pragma once

// 按页背景的管理者：每个页面一份背景视图，切页时交叉淡入（新的从 0 淡入、旧的垫在下面，
// 补间结束才 GONE）。
//
// 从 `Shell` 里抽出来的一件职责（原来它和图层、页头底栏、内容栈、弹窗、焦点框、玻璃
// 一起挤在一个 700 行的类里）。这个类**只认识 Box/View 和自己的补间时钟**：
// 不认识外壳、不认识主题、不认识图形 API，所以能单独读、单独测。
//
// 时间推进走 `step(dt)` 显式喂（测试可以给固定 dt，行为确定）；
// 外壳的 `update()` 每帧调一次。

#include <map>
#include <string>

#include "bklv/core/box.hpp"
#include "bklv/core/time.hpp"

namespace bklv
{

class BackdropManager
{
  public:
    // 背景挂到哪一层（外壳构造时给一次）。不给的话 add() 只登记、不进树。
    void setHostLayer(Box* layer) { this->hostLayer = layer; }

    // 淡入时长（秒）。<= 0 表示瞬切（不跑补间）。
    void setTransitionDuration(float seconds) { this->duration = seconds; }
    float getTransitionDuration() const { return this->duration; }

    // 登记一份页面背景。第一份登记的直接成为当前背景（否则"切页前"什么都没有）。
    void add(const std::string& pageId, View* backdrop);

    // 取某页的背景（没登记返回 nullptr）。
    View* get(const std::string& pageId) const;

    // 切到某页背景：交叉淡入。页面没登记背景时只警告、不改状态。
    void show(const std::string& pageId);

    // 推进淡入。补间跑完把旧背景 GONE 掉。
    void step(float deltaSeconds);

    View* getCurrent() const { return this->current; }
    const std::string& getCurrentId() const { return this->currentId; }
    bool isTransitioning() const { return !this->anim.isStopped(); }

  private:
    Box* hostLayer = nullptr;
    std::map<std::string, View*> backdrops;

    View* current = nullptr;
    View* previous = nullptr; // 淡入期间垫在下面的旧背景
    std::string currentId;
    float duration = 0.22f;
    Animatable anim{ 1.0f };
};

} // namespace bklv
