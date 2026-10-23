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
#include "bklv/core/services.hpp"

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

    // 独立运行 / 需要底层能力时的逃生口
    App& getApp();
    const App& getApp() const;

  private:
    Services services;
    bool servicesInstalled = false;
    std::unique_ptr<App> app;
};

} // namespace bklv
