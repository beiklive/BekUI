// 最小嵌入示例：宿主自己创建 SDL3 窗口 / GL 上下文 / ImGui 上下文与循环，
// bklv 只接收 ImGui 上下文和纹理后端，每帧 step() + draw()。
//
// 运行：dist/embed_imgui_gl（资源由 CMake POST_BUILD 复制到 dist/resources）

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <memory>

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_opengl3.h"

#include "bklv/backends/gl_loader.hpp"
#include "bklv/bklv.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/core/services.hpp"

namespace
{

// 宿主自己的纹理上传实现（这里用 GL）
class HostTextures : public bklv::TextureBackend
{
  public:
    const char* getName() const override { return "host-gl"; }
    bool isReady() const override { return true; }

    bklv::TextureHandle createTexture(const unsigned char* pixels, int width, int height) override
    {
        bklv::TextureHandle handle;
        if (pixels == nullptr || width <= 0 || height <= 0)
            return handle;

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
            pixels);

        handle.texture = (ImTextureID)(intptr_t)texture;
        handle.width   = width;
        handle.height  = height;
        return handle;
    }

    void destroyTexture(bklv::TextureHandle& handle) override
    {
        if (handle.texture == ImTextureID_Invalid)
            return;
        GLuint texture = (GLuint)(intptr_t)handle.texture;
        glDeleteTextures(1, &texture);
        handle = bklv::TextureHandle{};
    }
};

// 宿主的音频服务：接管所有 UI 音效（这里只打日志，实际可接自己的混音器）
class HostAudio : public bklv::AudioService
{
  public:
    void play(bklv::Sound sound) override
    {
        std::printf("host audio: sound=%d\n", (int)sound);
    }
    void playFile(const std::string& path) override
    {
        std::printf("host audio: file=%s\n", path.c_str());
    }
};

// 宿主的资源解析：从可执行文件旁的 resources/ 读（可替换成 pak/内存资源）
class HostResources : public bklv::ResourceProvider
{
  public:
    std::string resolve(const std::string& relative) const override
    {
        if (!relative.empty() && relative[0] == '/')
            return relative;
        const char* base = SDL_GetBasePath();
        return (base != nullptr ? std::string(base) : std::string()) + relative;
    }
};

class HostFonts : public bklv::FontProvider
{
  public:
    std::string resolveBodyFont() const override
    {
        const char* path = std::getenv("BKLV_HOST_FONT");
        return path != nullptr ? std::string(path) : std::string();
    }
};

class HostIme : public bklv::ImeManager
{
  public:
    bool open(const bklv::ImeRequest&) override { return false; } // 桌面宿主交给内联编辑
};

class HostInput : public bklv::InputService
{
    // 全部走默认：宿主没有额外输入源时保持 false
};

} // namespace

int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::fprintf(stderr, "SDL_Init 失败：%s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("bklv 嵌入示例", 1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
    {
        std::fprintf(stderr, "CreateWindow 失败：%s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (gl == nullptr || !bklv::loadGLFunctions())
    {
        std::fprintf(stderr, "GL 上下文/函数加载失败\n");
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);

    // ---- 宿主自己的 ImGui ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 150");

    // ---- bklv：先注入宿主服务，再创建 App（App 构造期会读 i18n/资源） ----
    HostAudio hostAudio;
    HostResources hostResources;
    HostFonts hostFonts;
    HostIme hostIme;
    HostInput hostInput;
    bklv::Services services;
    services.audio     = &hostAudio;
    services.resources = &hostResources;
    services.fonts     = &hostFonts;
    services.ime       = &hostIme;
    services.input     = &hostInput;
    bklv::setServices(services);

    bklv::App app;
    bklv::Box* root = new bklv::Box(bklv::Axis::COLUMN);
    root->setWidthPercentage(100.0f);
    root->setHeightPercentage(100.0f);
    root->setPadding(40.0f);
    root->setGap(16.0f);
    root->setBackgroundColor(IM_COL32(238, 242, 247, 255));

    bklv::Label* title = new bklv::Label();
    title->setText("bklv 嵌入模式");
    title->setFontSize(32.0f);
    title->setTextColor(IM_COL32(30, 40, 52, 255));
    root->addView(title);

    bklv::Label* hint = new bklv::Label();
    hint->setText("宿主拥有窗口/GL/ImGui；bklv 只负责 UI 树与 draw data。");
    hint->setTextColor(IM_COL32(80, 92, 104, 255));
    root->addView(hint);

    bklv::Image* image = new bklv::Image();
    image->setImageFromFile("resources/testcard.png");
    image->setWidth(320.0f);
    image->setHeight(180.0f);
    root->addView(image);

    bklv::Button* button = new bklv::Button("来自 bklv 的按钮");
    button->setOnClick([] { std::printf("bklv button clicked\n"); });
    root->addView(button);

    app.setRootView(root);

    HostTextures textures;
    bklv::HostAttachInfo attach;
    attach.imguiContext = ImGui::GetCurrentContext();
    attach.textures     = &textures;
    attach.width        = 1280.0f;
    attach.height       = 720.0f;
    if (!app.attachHost(attach))
    {
        std::fprintf(stderr, "bklv attachHost 失败\n");
        return 1;
    }

    bool running    = true;
    bool imageCheck = false;
    int frameCount  = 0;
    Uint64 last     = SDL_GetTicks();
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }

        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        if (width <= 0 || height <= 0)
            continue;
        app.setHostViewport((float)width, (float)height);

        ImGui_ImplSDL3_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        const Uint64 now = SDL_GetTicks();
        const float dt   = (float)(now - last) / 1000.0f;
        last             = now;

        app.step(dt);
        app.draw();

        if (!imageCheck && ++frameCount > 20)
        {
            imageCheck = true;
            std::printf("embed image loaded: %s\n", image->hasImage() ? "yes" : "no");
            std::fflush(stdout);
        }

        // 宿主自己的 ImGui 内容，与 bklv 共用同一帧
        ImGui::Begin("宿主窗口");
        ImGui::Text("这里由宿主绘制");
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, width, height);
        glClearColor(0.90f, 0.92f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    app.detachHost();
    bklv::clearServices();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
