#include "bklv/core/context.hpp"

namespace bklv
{
namespace
{

bool hasAnyService(const Services& services)
{
    return services.audio != nullptr || services.resources != nullptr
        || services.fonts != nullptr || services.ime != nullptr || services.input != nullptr;
}

} // namespace

Context::Context(const Services& services)
    : services(services)
{
    // 必须先装服务，再构造 App：App 构造期会初始化 i18n / 读资源
    if (hasAnyService(this->services))
    {
        setServices(this->services);
        this->servicesInstalled = true;
    }

    this->app = std::make_unique<App>();
}

Context::~Context()
{
    this->app.reset();
    if (this->servicesInstalled)
        clearServices();
}

void Context::setRootView(View* view)
{
    this->app->setRootView(view);
}

View* Context::getRootView() const
{
    return this->app->getRootView();
}

bool Context::attachHost(const HostAttachInfo& info)
{
    return this->app->attachHost(info);
}

void Context::setViewport(float width, float height)
{
    this->app->setHostViewport(width, height);
}

void Context::step(float deltaSeconds)
{
    this->app->step(deltaSeconds);
}

void Context::draw()
{
    this->app->draw();
}

void Context::detachHost()
{
    this->app->detachHost();
}

bool Context::isAttached() const
{
    return this->app->isHostAttached();
}

App& Context::getApp()
{
    return *this->app;
}

const App& Context::getApp() const
{
    return *this->app;
}

} // namespace bklv
