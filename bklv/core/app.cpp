#include "bklv/core/app.hpp"

#include <chrono>
#include "bklv/core/decode_queue.hpp"

#include "bklv/core/frame_budget.hpp"
#include "bklv/core/glass/glass_service.hpp"
#include "bklv/core/globals.hpp"
#include "bklv/core/image_writer.hpp"
#include "bklv/core/audio.hpp"
#include "bklv/core/services.hpp"
#include "bklv/core/task.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/debug_layer.hpp"
#include "bklv/widgets/notification.hpp"

#include "bklv/core/resource.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/core/ui_scale.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/views/label.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "bklv/widgets/widget_types.hpp"

#include <cstdlib>
#include <functional>
#include <memory>

#include "imgui.h"

#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/time.hpp"

namespace bklv
{

// 当前进程里的 App 实例（弹窗这类控件要挂到根视图上就得找到它）。
// 不在这里放 static：登记进全局表（core/globals.hpp），于是它也能被列举与重置。
constexpr const char* kAppGlobalSlot = "app";

// 最近一次布局使用的画布尺寸。
float App::lastLayoutWidth  = 0.0f;
float App::lastLayoutHeight = 0.0f;

const char* App::getGraphicsBackendName()
{
    return bklv::getGraphicsBackendName();
}

// ---------------------------------------------------------------------------
// 根视图
// ---------------------------------------------------------------------------

App::~App()
{
    if (this->hostAttached)
        this->detachHost();

    // 先停后台线程，别让 worker 在 App 析构后再回调
    task::shutdown();
    audio::shutdown();

    if (globals::getGlobalStore().find<App>(kAppGlobalSlot) == this)
        globals::getGlobalStore().reset(kAppGlobalSlot);

    // 先清掉输入层的焦点与模态栈：模态栈是进程级的静态，
    // 根视图一删，里面的指针就是野指针了（再导航/命中就踩崩）。
    getInputManager().resetFocusState();

    delete this->root;
    this->root = nullptr;
}

void App::setClearColor(float r, float g, float b)
{
    this->clearColor[0] = r;
    this->clearColor[1] = g;
    this->clearColor[2] = b;
}

void App::setRootView(View* view)
{
    // 通知层/调试层挂在旧根上，换根后指针失效（和模态栈一样必须清掉）
    this->notifications = nullptr;
    this->debugLayer    = nullptr;

    if (this->root == view)
        return;

    // 换根视图 = 旧树整棵作废，输入层里指向旧树的焦点/模态必须先清掉
    getInputManager().resetFocusState();

    delete this->root;
    this->root = view;

    if (this->root != nullptr)
    {
        this->root->setDetached(true);
        this->root->setId("bklv/root");
    }

    // 焦点导航从根视图开始。
    getInputManager().setRoot(this->root);
    if (this->root != nullptr)
        getInputManager().giveFocus(this->root->getDefaultFocus());

    // 窗口/根视图就绪前请求过调试层的话，现在补上
    if (this->debugLayerRequested)
        this->setDebugLayerEnabled(true);

    // 补发排队中的通知（在此之前 App::notify 只能先记下来）
    if (!this->pendingNotifications.empty())
    {
        std::vector<std::pair<std::string, float>> queued;
        queued.swap(this->pendingNotifications);
        for (const auto& item : queued)
            this->notify(item.first, item.second);
    }
}


// ---------------------------------------------------------------------------
// 帧
// ---------------------------------------------------------------------------

namespace
{

// 需要重新布局的标志。首帧为 true：还没算过任何一次布局。
bool g_layoutDirty = true;

// 累计布局次数。用来验证"静态页面不再每帧白算一次布局"。
unsigned int g_layoutCount = 0;

// 排查漏置脏用的逃生开关：置上就退回"每帧无条件全树重算"的老行为。
bool forceRelayout()
{
    static const bool enabled = std::getenv("BKLV_FORCE_RELAYOUT") != nullptr;
    return enabled;
}

} // namespace

bool App::setTheme(const std::string& name)
{
    if (!bklv::setTheme(name))
        return false;

    // 外观是按（主题, 控件）缓存的，上面已经清掉；这里让每个控件重新取外观。
    // 控件的 applyTheme() 只覆盖"没被显式设置过"的值。
    if (this->root != nullptr)
        this->root->applyThemeRecursive();

    // 主题可能改了间距/字号，必须重排
    App::requestLayout();
    return true;
}

void App::requestLayout()
{
    g_layoutDirty = true;
}

void App::noteLayoutPerformed()
{
    g_layoutCount++;
}

unsigned int App::getLayoutCount()
{
    return g_layoutCount;
}

namespace
{
App* g_contextApp = nullptr;
}

void App::setContextApp(App* app)
{
    g_contextApp = app;
}

App* App::getContextApp()
{
    return g_contextApp;
}

App* App::getCurrent()
{
    if (g_contextApp != nullptr)
        return g_contextApp;
    return globals::getGlobalStore().find<App>(kAppGlobalSlot);
}

NotificationManager* App::getNotifications()
{
    if (this->root == nullptr)
        return nullptr;

    if (this->notifications == nullptr)
    {
        this->notifications = new NotificationManager();
        // 右上角浮层：绝对定位，不参与根视图的 flex 排布
        this->notifications->setPositionType(PositionType::ABSOLUTE);
        this->notifications->setPositionTop(getTheme().number("metric.notification.marginTop", 16.0f));
        this->notifications->setPositionRight(
            getTheme().number("metric.notification.marginRight", 16.0f));

        if (Box* rootBox = this->root->asBox())
            rootBox->addView(this->notifications);
        else
        {
            BKLV_LOG_WARNING("App::notify 失败：根视图不是容器");
            this->notifications = nullptr;
            return nullptr;
        }
    }

    return this->notifications;
}

void App::notify(const std::string& text, float duration)
{
    NotificationManager* manager = this->getNotifications();
    if (manager == nullptr)
    {
        // 根视图还没设好（窗口创建前 notify）：先排队，setRootView 之后补发
        this->pendingNotifications.emplace_back(text, duration);
        return;
    }

    manager->notify(text, duration);
    this->requestLayout();
}

void App::saveScreenshot(RenderBackend* backend, float width, float height)
{
    // 读的是**像素**，不是逻辑尺寸：Retina 这类 2x 屏上帧缓冲是逻辑尺寸的两倍，
    // 按逻辑尺寸读只会拿到左下角那一块（截出来的图像是被放大了 4 倍）。踩过。
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    const int pixelWidth  = (int)(width * (scale.x > 0.0f ? scale.x : 1.0f));
    const int pixelHeight = (int)(height * (scale.y > 0.0f ? scale.y : 1.0f));

    std::vector<unsigned char> pixels;
    if (!backend->readPixels(0, 0, pixelWidth, pixelHeight, pixels))
    {
        BKLV_LOG_ERROR("后端不支持帧读回，截图失败：%s", this->screenshotPath.c_str());
        return;
    }

    if (!writePngFile(this->screenshotPath, pixels.data(), pixelWidth, pixelHeight, 4))
        BKLV_LOG_ERROR("截图写文件失败：%s", this->screenshotPath.c_str());
    else
        BKLV_LOG_INFO("截图已保存：%s（%dx%d 像素）", this->screenshotPath.c_str(), pixelWidth,
            pixelHeight);
}

bool App::requestScreenshot(const std::string& path, int delayFrames)
{
    if (path.empty())
        return false;

    this->screenshotPath  = path;
    this->screenshotDelay = delayFrames < 0 ? 0 : delayFrames;
    return true;
}

void App::setDebugLayerEnabled(bool enabled)
{
    // 根视图还没就绪（窗口创建前就调用）：先记下来，setRootView 之后再真正创建。
    // 静默忽略过一次，结果"开了调试层却什么都看不到"（踩过）。
    this->debugLayerRequested = enabled;

    if (this->root == nullptr)
        return;

    Box* rootBox = this->root->asBox();
    if (rootBox == nullptr)
        return;

    if (enabled && this->debugLayer == nullptr)
    {
        this->debugLayer = new DebugLayer();
        this->debugLayer->setRoot(this->root);
        rootBox->addView(this->debugLayer); // 最后一个子节点 → 画在最上层
    }
    else if (!enabled && this->debugLayer != nullptr)
    {
        rootBox->removeView(this->debugLayer, true);
        this->debugLayer = nullptr;
    }
}

bool App::pushOverlay(View* overlay, bool closeOnBack)
{
    if (overlay == nullptr || this->root == nullptr)
        return false;

    Box* rootBox = this->root->asBox();
    if (rootBox == nullptr)
    {
        BKLV_LOG_ERROR("根视图不是容器，挂不了弹层");
        return false;
    }

    // 铺满画布、盖在最上层（后加的子节点后绘制）
    overlay->setPositionType(PositionType::ABSOLUTE);
    overlay->setPositionTop(0.0f);
    overlay->setPositionLeft(0.0f);
    overlay->setWidthPercentage(100.0f);
    overlay->setHeightPercentage(100.0f);

    rootBox->addView(overlay);
    getInputManager().pushModal(overlay, closeOnBack);
    return true;
}

void App::popOverlay(View* overlay, bool free)
{
    if (overlay == nullptr || this->root == nullptr)
        return;

    // 只在它确实是当前最上层模态时才弹栈，避免栈错位
    if (getInputManager().getTopModal() == overlay)
        getInputManager().popModal();

    if (Box* rootBox = this->root->asBox())
        rootBox->removeView(overlay, free);
}

App::App()
{
    // 让页面 JSON 也能用外观驱动的控件（Button / Rectangle / Shell …）
    registerWidgetViewTypes();

    globals::getGlobalStore().set<App>(kAppGlobalSlot, this, false); // 借用指针：App 自己拥有自己
    // 框架里最早能执行的用户代码。Switch 上没有终端，先把日志落到 SD 卡，
    // 这样后面任何一步失败（甚至崩溃）都能留下线索。
    enablePlatformLog();
    initResources();
    // 文案要在视图树之前就绪：demo 是在 App::run() 之前把树建好的。
    initI18n();
    // 音效设备（失败不阻塞：没声音也要能用）；宿主注入了 AudioService 就不再开内置设备
    if (getServices().audio == nullptr)
        audio::init();
    BKLV_LOG_INFO("bklv 启动");
}

void App::beginFrameWork()
{
    // 帧预算：只管"可以延后"的活（后台回调、上传、动图换帧、SVG 栅格化、缓存淘汰）。
    budget::beginFrame(this->frameBudgetMs);

    // 后台任务完成的回调统一在主线程执行（视图/纹理/ImGui 都只能主线程碰）
    task::pumpMainThread();
    pumpDecodeQueue(); // 背压：按并发上限启动解码
    pumpUploads();     // 背压：按每帧配额 + 帧预算执行上传
    trimTextureCache();
}

void App::updateScene(float width, float height)
{
    const auto tStart = std::chrono::steady_clock::now();

    if (this->root == nullptr)
        return;

    // 0. 字体是在窗口创建后才加载的，而视图树通常更早就建好了。
    //    字体就位后必须让所有依赖文本尺寸的节点重算一次。
    if (this->lastFontGeneration != getFontGeneration())
    {
        this->lastFontGeneration = getFontGeneration();
        this->root->refreshFontDependentLayout();
    }

    // 1. 布局：**只在需要时算**。
    App::lastLayoutWidth  = width;
    App::lastLayoutHeight = height;

    const bool canvasChanged = (this->lastCanvasWidth != width || this->lastCanvasHeight != height);

    // 720p 缩放：主题尺寸按 1280x720 写，窗口宽度变了要重新算缩放。
    if (canvasChanged && updateUiScaleFromCanvas(width, height))
    {
        this->root->applyThemeRecursive();
        this->root->markMeasureDirtyRecursive();
        g_layoutDirty = true;
    }

    auto layoutPass = [&]() {
        g_layoutDirty          = false;
        this->lastCanvasWidth  = width;
        this->lastCanvasHeight = height;

        YGNodeCalculateLayout(this->root->getYGNode(), width, height, YGDirectionLTR);
        App::noteLayoutPerformed();
        this->root->notifyLayout();
    };

    if (forceRelayout() || g_layoutDirty || canvasChanged)
        layoutPass();

    // 2. 动画与视图逐帧逻辑（滚动等）。
    Ticking::updateTickings();
    const auto tAfterLayout = std::chrono::steady_clock::now();
    this->root->update();
    const auto tAfterUpdate = std::chrono::steady_clock::now();

    // 3. 输入与焦点（必须在绘制前，读取的是上一帧的命中结果）。
    getInputManager().update();

    // 4. 业务帧回调。
    if (this->onUpdate)
        this->onUpdate();

    // 4b. 补一次布局：2/3/4 任何一步都可能新建或改动视图树。
    if (g_layoutDirty)
        layoutPass();

    this->profile.layoutMs += std::chrono::duration<double, std::milli>(
        tAfterLayout - tStart).count();
    this->profile.updateMs += std::chrono::duration<double, std::milli>(
        tAfterUpdate - tAfterLayout).count();
}

void App::drawScene()
{
    const auto tStart = std::chrono::steady_clock::now();

    if (this->root != nullptr)
    {
        FrameContext ctx;
        // 画在主视口的前景绘制列表上：不依赖任何 ImGui 窗口，也不会被窗口遮挡。
        ctx.drawList   = ImGui::GetForegroundDrawList();
        ctx.pixelRatio = 1.0f;

        // 5. 绘制视图树。
        this->root->frame(&ctx);

        // 7. ImGui 窗口（调试窗口、日志窗口）。
        if (this->onDrawWindows)
            this->onDrawWindows();
    }

    this->profile.drawMs += std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - tStart).count();
    this->profile.frames++;

    // 预算在这一帧结束（它覆盖了 root->update() 里的动图换帧与 SVG 栅格化）
    budget::endFrame();
}

void App::frame()
{
    this->beginFrameWork();

    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    this->updateScene(viewport->WorkSize.x, viewport->WorkSize.y);
    this->drawScene();

    ImGui::Render();
}

// ---------------------------------------------------------------------------
// 嵌入模式
// ---------------------------------------------------------------------------

namespace
{
// 独立运行：把 RenderBackend 的纹理能力适配成宿主接口（宿主模式由调用方实现）
class BackendTextureAdapter : public TextureBackend
{
  public:
    explicit BackendTextureAdapter(RenderBackend* backend)
        : backend(backend)
    {
    }

    const char* getName() const override { return this->backend->getName(); }
    TextureHandle createTexture(const unsigned char* pixels, int width, int height) override
    {
        return this->backend->createTexture(pixels, width, height);
    }
    bool updateTexture(TextureHandle& handle, const unsigned char* pixels, int width,
        int height) override
    {
        return this->backend->updateTexture(handle, pixels, width, height);
    }
    void destroyTexture(TextureHandle& handle) override
    {
        this->backend->destroyTexture(handle);
    }
    bool isReady() const override { return this->backend->isReady(); }

  private:
    RenderBackend* backend = nullptr;
};
} // namespace

bool App::attachHost(const HostAttachInfo& info)
{
    if (this->hostAttached || info.imguiContext == nullptr || info.textures == nullptr)
        return false;

    if (ImGui::GetCurrentContext() != info.imguiContext)
        ImGui::SetCurrentContext(info.imguiContext);

    this->hostAttached = true;
    this->hostContext  = info.imguiContext;
    this->hostTextures = info.textures;
    this->hostWidth    = info.width > 0.0f ? info.width : 1280.0f;
    this->hostHeight   = info.height > 0.0f ? info.height : 720.0f;

    // 字体必须在宿主第一次 NewFrame() 之前加入图集（调用方契约）
    loadFonts();
    initTextureStore(info.textures);

    // 嵌入模式没有 bklv 后端的玻璃实现：整层退回半透明。
    glass::getGlassService().setBackend(nullptr);

    App::requestLayout();
    return true;
}

void App::setHostViewport(float width, float height)
{
    if (!this->hostAttached || width <= 0.0f || height <= 0.0f)
        return;
    if (this->hostWidth == width && this->hostHeight == height)
        return;
    this->hostWidth  = width;
    this->hostHeight = height;
    App::requestLayout();
}

void App::step(float deltaSeconds)
{
    if (!this->hostAttached || ImGui::GetCurrentContext() == nullptr)
        return;

    if (deltaSeconds > 0.0f)
        ImGui::GetIO().DeltaTime = deltaSeconds;

    this->beginFrameWork();
    this->updateScene(this->hostWidth, this->hostHeight);
}

void App::draw()
{
    if (!this->hostAttached || ImGui::GetCurrentContext() == nullptr)
        return;
    this->drawScene();
}

void App::detachHost()
{
    if (!this->hostAttached)
        return;

    shutdownTextureStore();
    this->hostAttached = false;
    this->hostContext  = nullptr;
    this->hostTextures = nullptr;
}

// ---------------------------------------------------------------------------
// 主循环
// ---------------------------------------------------------------------------

int App::run(const std::string& title, int width, int height)
{
    // 平台负责窗口、事件循环与图形后端（后端在平台内部按编译期选择创建）。
    BKLV_LOG_INFO("创建平台");
    std::unique_ptr<Platform> platform(createPlatform(this->platformType));
    if (platform == nullptr)
    {
        Logger::error("平台初始化失败");
        return 1;
    }

    // ImGui 上下文必须先于平台建窗口：
    // 图形上下文初始化会顺带初始化 ImGui 的 GL 后端（创建字体图集纹理），
    // 以及 ImGui 平台后端的初始化，两者都要求上下文已存在。
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // 必须在第一次 NewFrame 之前设置：ImGui 只在 io.IniFilename 非空时才读写 .ini。
    // 置空后既不会读取已有的 imgui.ini，也不会在退出/定时保存时生成新的。
    if (std::getenv("BKLV_USE_INI") != nullptr)
        this->iniPersistenceEnabled = true;

    if (!this->iniPersistenceEnabled)
        ImGui::GetIO().IniFilename = nullptr;

    if (!platform->createWindow(title, (uint32_t)width, (uint32_t)height))
    {
        BKLV_LOG_ERROR("窗口/图形上下文创建失败，退出");
        ImGui::DestroyContext();
        return 1;
    }
    BKLV_LOG_INFO("窗口与图形上下文就绪");

    loadFonts();

    RenderBackend* backend = platform->getGraphicsBackend();

    // 图形 API 那一侧的渲染后端。
    if (backend == nullptr || !backend->initImGuiRenderer())
    {
        BKLV_LOG_ERROR("ImGui 渲染后端初始化失败，退出");
        ImGui::DestroyContext();
        return 1;
    }
    BKLV_LOG_INFO("ImGui 渲染后端就绪，进入主循环");

    // 到这里上下文与渲染后端都就绪了，图片可以上传纹理了。
    this->ownedTextureBackend = std::make_unique<BackendTextureAdapter>(backend);
    initTextureStore(this->ownedTextureBackend.get());

    // 玻璃服务要后端交出来的玻璃实现（没有玻璃能力的后端给 nullptr = 整层关闭）。
    // 两边都是非拥有指针：退出前摘掉，见函数末尾。
    this->renderBackend = backend;
    glass::getGlassService().setBackend(backend->getGlassBackend());

    const float clearColor[4] = { this->clearColor[0], this->clearColor[1], this->clearColor[2],
        1.0f };

    // 平台自己的循环包装：Switch 上需要先做 applet / EGL 初始化。
    platform->runLoop([&]() {
        while (!this->quitRequested && platform->mainLoopIteration())
        {
            // 帧时间统计：量整个迭代（含 present），"卡不卡"看它的 p99 / 最坏帧
            const double frameStart = getSecondsNow();

            backend->newFrame();

            // 玻璃：帧首清队列并记下帧缓冲尺寸（物理像素）与密度。
            // 必须在 this->frame() 之前 —— 视图是在那一趟里声明"我要玻璃"的。
            {
                const ImGuiIO& io = ImGui::GetIO();
                const float scaleX = io.DisplayFramebufferScale.x > 0.0f
                    ? io.DisplayFramebufferScale.x : 1.0f;
                const float scaleY = io.DisplayFramebufferScale.y > 0.0f
                    ? io.DisplayFramebufferScale.y : 1.0f;
                glass::getGlassService().beginFrame((int)(io.DisplaySize.x * scaleX),
                    (int)(io.DisplaySize.y * scaleY), scaleX);
            }

            this->frame();
            backend->clear(clearColor);
            // 宿主可以在这里改 draw data（设计器的液态玻璃注入玻璃命令）
            if (this->onPrepareDrawData)
                this->onPrepareDrawData(ImGui::GetDrawData());

            // 玻璃注入：排在宿主钩子之后，宿主对绘制数据的改动也能被采进背景
            if (ImDrawData* drawData = ImGui::GetDrawData())
                glass::getGlassService().prepareDrawData(*drawData);

            backend->renderDrawData(ImGui::GetDrawData());
            glass::getGlassService().finishFrame();

            // 截图要在交换之前读后缓冲（交换之后内容未定义）
            if (!this->screenshotPath.empty())
            {
                if (this->screenshotDelay > 0)
                {
                    this->screenshotDelay--;
                }
                else
                {
                    // 实际可绘制区域可能比请求的小（窗口被屏幕限制、或窗口被用户拖小）：
                    // 按 ImGui 的 DisplaySize 读，否则会读到缓冲区外、图错位（踩过）
                    const ImVec2 display = ImGui::GetIO().DisplaySize;
                    const float shotWidth  = display.x > 0.0f ? display.x : width;
                    const float shotHeight = display.y > 0.0f ? display.y : height;
                    this->saveScreenshot(backend, shotWidth, shotHeight);
                    this->screenshotPath.clear();
                    this->quitRequested = true;
                }
            }

            backend->present();

            this->frameStats.record(getSecondsNow() - frameStart);
        }
        return true;
    });

    // 释放顺序很重要：先释放纹理（还要用上下文），再关渲染后端，最后销毁平台。
    shutdownTextureStore();
    this->ownedTextureBackend.reset();

    // 退出一条帧时间流水：每个程序都自动有一条"卡不卡"的验收数据（没有帧就没这行）
    {
        const FrameTimeSummary summary = this->frameStats.getSummary();
        if (summary.count > 0)
        {
            Logger::info("帧时间：" + std::to_string((int)summary.p50Ms) + "ms(p50) / "
                + std::to_string((int)summary.p99Ms) + "ms(p99) / 最坏 "
                + std::to_string((int)summary.worstMs) + "ms；掉帧 "
                + std::to_string(summary.droppedCount) + "/" + std::to_string(summary.count)
                + "（60Hz 一帧 16.7ms 为线）");

            const double n = (double)this->profile.frames;
            if (n > 0.0)
                Logger::info("帧内分段（均值）：布局 " + std::to_string(this->profile.layoutMs / n)
                    + "ms / update " + std::to_string(this->profile.updateMs / n)
                    + "ms / 绘制 " + std::to_string(this->profile.drawMs / n) + "ms");
        }
    }

    // 玻璃服务与宿主持的都是后端的非拥有指针：后端随平台一起销毁，先把它们摘掉。
    glass::getGlassService().setBackend(nullptr);
    this->renderBackend = nullptr;

    backend->shutdownImGuiRenderer();
    platform->shutdownImGuiPlatformBackend();
    ImGui::DestroyContext();

    // 平台析构负责销毁窗口与图形上下文。
    platform.reset();

    return 0;
}

int RunDemo(const std::string& title, View* root, int width, int height)
{
    App app;
    app.setRootView(root);
    return app.run(title, width, height);
}

} // namespace bklv
