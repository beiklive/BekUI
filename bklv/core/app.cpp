#include "bklv/core/app.hpp"

#include "bklv/core/resource.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/core/i18n.hpp"

#include <cstdlib>
#include <memory>

#include "imgui.h"

#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/time.hpp"
#include "bklv/views/hints.hpp"

namespace bklv
{

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
    delete this->root;
    this->root = nullptr;
}

void App::setRootView(View* view)
{
    if (this->root == view)
        return;

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

App::App()
{
    // 框架里最早能执行的用户代码。Switch 上没有终端，先把日志落到 SD 卡，
    // 这样后面任何一步失败（甚至崩溃）都能留下线索。
    enablePlatformLog();
    initResources();
    // 文案要在视图树之前就绪：demo 是在 App::run() 之前把树建好的。
    initI18n();
    BKLV_LOG_INFO("bklv 启动");
}

void App::frame()
{
    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float width             = viewport->WorkSize.x;
    const float height            = viewport->WorkSize.y;

    FrameContext ctx;
    // 画在主视口的前景绘制列表上：不依赖任何 ImGui 窗口，也不会被窗口遮挡。
    ctx.drawList   = ImGui::GetForegroundDrawList();
    ctx.theme      = &getDefaultTheme();
    ctx.pixelRatio = 1.0f;

    if (this->root != nullptr)
    {
        // 0. 字体是在窗口创建后才加载的，而视图树通常更早就建好了。
        //    字体就位后必须让所有依赖文本尺寸的节点重算一次。
        if (this->lastFontGeneration != getFontGeneration())
        {
            this->lastFontGeneration = getFontGeneration();
            this->root->refreshFontDependentLayout();
        }

        // 1. 布局：**只在需要时算**。
        //
        //    以前是每帧无条件全树重算（保守但白烧）：静态页面每秒白算 60 次 Yoga。
        //    触发条件只有三个：
        //      - 画布尺寸变了（resize 这类外部原因不经过 invalidate）
        //      - requestLayout() / 首帧
        //      - 视图自己改了：那些 setter 内部会 invalidate()，冒泡到根**立即**重算
        //        （所以不在这里排队，也就不会重复算）
        App::lastLayoutWidth  = width;
        App::lastLayoutHeight = height;

        const bool canvasChanged = (this->lastCanvasWidth != width || this->lastCanvasHeight != height);
        if (forceRelayout() || g_layoutDirty || canvasChanged)
        {
            g_layoutDirty         = false;
            this->lastCanvasWidth = width;
            this->lastCanvasHeight = height;

            YGNodeCalculateLayout(this->root->getYGNode(), width, height, YGDirectionLTR);
            App::noteLayoutPerformed();
            // 布局变化要通知子树（滚动条夹取、tab 栏反推内容宽等都挂在这里）
            this->root->notifyLayout();
        }



        // 2. 动画与视图逐帧逻辑（滚动等）。
        Ticking::updateTickings();
        this->root->update();

        // 3. 输入与焦点（必须在绘制前，读取的是上一帧的命中结果）。
        getInputManager().update();

        // 4. 业务帧回调。
        if (this->onUpdate)
            this->onUpdate();

        // 5. 绘制视图树。
        this->root->frame(&ctx);

        // 6. 提示栏画在最上层。
        drawHints(ctx.drawList, width, height);

        // 7. ImGui 窗口（调试窗口、日志窗口）。
        //    放在视图树之后，所以窗口浮在界面之上；必须在 ImGui::Render() 之前。
        if (this->onDrawWindows)
            this->onDrawWindows();
    }

    ImGui::Render();
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
    initTextureStore(backend);

    const float clearColor[4] = { 0.10f, 0.10f, 0.12f, 1.0f };

    // 平台自己的循环包装：Switch 上需要先做 applet / EGL 初始化。
    platform->runLoop([&]() {
        while (platform->mainLoopIteration())
        {
            backend->newFrame();
            this->frame();
            backend->clear(clearColor);
            backend->renderDrawData(ImGui::GetDrawData());
            backend->present();
        }
        return true;
    });

    // 释放顺序很重要：先释放纹理（还要用上下文），再关渲染后端，最后销毁平台。
    shutdownTextureStore();

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
