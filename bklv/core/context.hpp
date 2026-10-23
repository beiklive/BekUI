#pragma once

// UI 上下文（批次 B：单实例嵌入 + 后续多实例的挂载点）。
//
// 现在 App 仍是运行时本体；Context 负责"谁能拥有它"的公开形状：
//   · 构造时注入 Services（必须早于 App 构造，App 构造期会读 i18n/资源）；
//   · 拥有 root View 的生命周期；
//   · 宿主模式 attach/step/draw/detach；
//   · 后续批次会把主题/焦点/模态/输入从全局迁移到这里。
//
// 单实例限制：App 内部仍有全局状态，一个进程只建议创建一个 Context。

#include <memory>

#include "bklv/core/app.hpp"
#include "bklv/core/event.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/core/services.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

class Context
{
  public:
    explicit Context(const Services& services = Services{});
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    // root 由 Context（底层 App）接管所有权
    void setRootView(View* view);
    View* getRootView() const;

    // 宿主嵌入
    bool attachHost(const HostAttachInfo& info);
    void setViewport(float width, float height);
    void step(float deltaSeconds = 0.0f);
    void draw();
    void detachHost();
    bool isAttached() const;
    void prepareDrawData(ImDrawData& drawData);
    void finishFrame();

    // 每个 Context 独立的主题 / 输入 / 事件总线
    ThemeData& getThemeData() { return this->theme; }
    InputManager& getInputState() { return this->input; }
    EventBus& getEventBusState() { return this->bus; }

    // 这个 Context 私有的 JSON 视图类型（扩展控件）
    void addViewType(const std::string& name, std::function<View*()> creator)
    {
        this->viewTypes[name] = std::move(creator);
    }
    bool loadTheme(const std::string& name) { return this->theme.load(name); }

    // 让这个 Context 成为当前线程的"当前 UI"（输入/主题查找走它）。
    // 构造、setRootView、step、draw 会自动调用；多实例时构建视图前也要显式调用。
    void makeCurrent();

    // 独立运行 / 需要底层能力时的逃生口
    App& getApp();
    const App& getApp() const;

  private:
    Services services;
    bool servicesInstalled = false;
    ThemeData theme;
    InputManager input;
    EventBus bus;
    ViewTypeMap viewTypes;
    std::unique_ptr<App> app;
};

} // namespace bklv
