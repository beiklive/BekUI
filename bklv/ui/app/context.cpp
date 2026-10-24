#include "bklv/ui/app/context.hpp"

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

    // 每个 Context 独立主题与输入/焦点/模态状态
    this->theme.load("");
    this->makeCurrent();

    this->app = std::make_unique<App>();
    App::setContextApp(this->app.get());
}

Context::~Context()
{
    // 先析构 App：它的收尾要用当前 Context 的输入/主题状态
    this->app.reset();

    if (getContextTheme() == &this->theme)
        setContextTheme(nullptr);
    if (getContextInputManager() == &this->input)
        setContextInputManager(nullptr);
    if (getContextEventBus() == &this->bus)
        setContextEventBus(nullptr);
    if (getContextViewTypes() == &this->viewTypes)
        setContextViewTypes(nullptr);
    if (this->servicesInstalled)
        clearServices();
}

void Context::makeCurrent()
{
    setContextTheme(&this->theme);
    setContextInputManager(&this->input);
    setContextEventBus(&this->bus);
    setContextViewTypes(&this->viewTypes);
    if (this->app != nullptr)
        App::setContextApp(this->app.get());
}

void Context::setRootView(View* view)
{
    this->makeCurrent();
    this->app->setRootView(view);
}

View* Context::getRootView() const
{
    return this->app->getRootView();
}

bool Context::attachHost(const HostAttachInfo& info)
{
    this->makeCurrent();
    return this->app->attachHost(info);
}

void Context::setViewport(float width, float height)
{
    this->app->setHostViewport(width, height);
}

void Context::step(float deltaSeconds)
{
    this->makeCurrent();
    this->app->step(deltaSeconds);
}

void Context::draw()
{
    this->makeCurrent();
    this->app->draw();
}

void Context::detachHost()
{
    this->app->detachHost();
}

void Context::prepareDrawData(ImDrawData& drawData)
{
    this->app->prepareDrawData(drawData);
}

void Context::finishFrame()
{
    this->app->finishHostFrame();
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
