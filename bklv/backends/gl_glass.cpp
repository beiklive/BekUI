// 桌面 GL 的玻璃后端：只在 GL 构建里编；其它后端给出「能力为空」的桩，
// 这样服务层不必到处写 #if。
#include "bklv/backends/gl_glass.hpp"

#include "bklv/core/logger.hpp"

#if defined(BKLV_OPENGL)
#include "bklv/glass/glass_blur_plan.hpp"
#include "bklv/glass/glass_geometry.hpp"
#include "bklv/glass/glass_request.hpp"
#include "bklv/backends/gl_glass.hpp"


#include "bklv/glass/glass_blur_plan.hpp"
#include "bklv/backends/gl_glass_procs.hpp"
#include "bklv/backends/gl_glass_shaders.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace bklv::glass
{

namespace
{

constexpr uint32_t kMaxGlassRegions  = 32;
constexpr float kMaximumBlurRadius   = 96.0f;

float clampFinite(float value, float minimum, float maximum, float fallback) noexcept
{
    return std::isfinite(value) ? std::min(std::max(value, minimum), maximum) : fallback;
}

bool hasVisibleRequest(const GlassRequestQueue& queue) noexcept
{
    for (const GlassRequest& request : queue.getRequests())
    {
        if (request.opacity > 0.0f && request.minMax.z > request.minMax.x
            && request.minMax.w > request.minMax.y)
            return true;
    }
    return false;
}

GLuint compileShader(GLenum type, const char* source, const char* name)
{
    GlProcs& gl = glProcs();
    const GLuint shader = gl.CreateShader(type);
    gl.ShaderSource(shader, 1, &source, nullptr);
    gl.CompileShader(shader);

    GLint success = GL_FALSE;
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE)
        return shader;

    GLint logLength = 0;
    gl.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::string message((size_t)std::max(logLength, 1), '\0');
    gl.GetShaderInfoLog(shader, logLength, nullptr, message.data());
    BKLV_LOG_ERROR("[玻璃] 着色器编译失败（%s）：%s", name, message.c_str());
    gl.DeleteShader(shader);
    return 0;
}

GLuint linkProgram(const char* vertexSource, const char* fragmentSource, const char* name)
{
    GlProcs& gl = glProcs();
    const GLuint vertex   = compileShader(GL_VERTEX_SHADER, vertexSource, name);
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, name);
    if (vertex == 0 || fragment == 0)
    {
        if (vertex != 0)
            gl.DeleteShader(vertex);
        if (fragment != 0)
            gl.DeleteShader(fragment);
        return 0;
    }

    const GLuint program = gl.CreateProgram();
    gl.AttachShader(program, vertex);
    gl.AttachShader(program, fragment);
    gl.LinkProgram(program);
    gl.DeleteShader(vertex);
    gl.DeleteShader(fragment);

    GLint success = GL_FALSE;
    gl.GetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_TRUE)
        return program;

    GLint logLength = 0;
    gl.GetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    std::string message((size_t)std::max(logLength, 1), '\0');
    gl.GetProgramInfoLog(program, logLength, nullptr, message.data());
    BKLV_LOG_ERROR("[玻璃] 着色器链接失败（%s）：%s", name, message.c_str());
    gl.DeleteProgram(program);
    return 0;
}

struct BlurUniforms
{
    GLint source = -1;
    GLint inverseSourceResolution = -1;
    GLint direction = -1;
    GLint centerWeight = -1;
    GLint pairCount = -1;
    GLint passKind = -1;
    GLint pairWeights = -1;
    GLint pairOffsets = -1;
};

struct GlassUniforms
{
    GLint backdrop = -1;
    GLint rawBackdrop = -1;
    GLint bounds = -1;
    GLint radii = -1;
    GLint refraction = -1;
    GLint filter = -1;
    GLint tint = -1;
    GLint framebuffer = -1;
    GLint deformation = -1;
    GLint lighting = -1;
    GLint light = -1;
    GLint framebufferSize = -1;
};

bool createTarget(const char* name, uint32_t width, uint32_t height, GLuint& texture,
    GLuint& framebuffer)
{
    GlProcs& gl = glProcs();

    gl.GenTextures(1, &texture);
    gl.BindTexture(GL_TEXTURE_2D, texture);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // 桌面 GL 3.2 没有 glTexStorage2D（4.2 才有，macOS 也上不去），用 glTexImage2D
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)width, (GLsizei)height, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, nullptr);
    if (gl.GetError() != GL_NO_ERROR)
    {
        BKLV_LOG_ERROR("[玻璃] %s 纹理创建失败（%ux%u）", name, width, height);
        return false;
    }

    gl.GenFramebuffers(1, &framebuffer);
    gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    const GLenum status = gl.CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        BKLV_LOG_ERROR("[玻璃] %s 帧缓冲不完整（0x%x）", name, (unsigned)status);
        return false;
    }
    return true;
}

} // namespace

struct GlGlassBackend::Impl
{
    int width  = 0;
    int height = 0;
    uint32_t blurWidth = 0;
    uint32_t blurHeight = 0;

    GLuint captureTexture = 0;
    GLuint captureFramebuffer = 0;
    GLuint contentCaptureTexture = 0;
    GLuint contentCaptureFramebuffer = 0;
    GLuint pingTexture = 0;
    GLuint pingFramebuffer = 0;
    GLuint pongTexture = 0;
    GLuint pongFramebuffer = 0;

    GLuint blurProgram = 0;
    GLuint glassProgram = 0;
    GLuint vertexArray = 0;

    BlurUniforms blur{};
    GlassUniforms glass{};

    bool prepared = false;
    bool disabled = false;
    int failedWidth = 0;
    int failedHeight = 0;
    GlassBackdropSource activeSource = GlassBackdropSource::FRAME;

    void destroy();
    bool renderBlur(GLuint source, const GlassBlurPlan& plan);
    void renderBlurPass(GLuint source, GLuint framebuffer, uint32_t sourceWidth,
        uint32_t sourceHeight, float passKind, float directionX, float directionY,
        const GlassBlurPlan& plan);
};

void GlGlassBackend::Impl::destroy()
{
    GlProcs& gl = glProcs();
    if (this->blurProgram != 0)
        gl.DeleteProgram(this->blurProgram);
    if (this->glassProgram != 0)
        gl.DeleteProgram(this->glassProgram);
    if (this->vertexArray != 0)
        gl.DeleteVertexArrays(1, &this->vertexArray);

    const GLuint textures[4] = { this->captureTexture, this->contentCaptureTexture,
        this->pingTexture, this->pongTexture };
    gl.DeleteTextures(4, textures);

    const GLuint framebuffers[4] = { this->captureFramebuffer,
        this->contentCaptureFramebuffer, this->pingFramebuffer, this->pongFramebuffer };
    gl.DeleteFramebuffers(4, framebuffers);

    *this = Impl{};
}

void GlGlassBackend::Impl::renderBlurPass(GLuint source, GLuint framebuffer,
    uint32_t sourceWidth, uint32_t sourceHeight, float passKind, float directionX,
    float directionY, const GlassBlurPlan& plan)
{
    GlProcs& gl = glProcs();

    gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    gl.Viewport(0, 0, (GLsizei)this->blurWidth, (GLsizei)this->blurHeight);
    gl.Disable(GL_BLEND);
    gl.Disable(GL_CULL_FACE);
    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_STENCIL_TEST);
    gl.Disable(GL_SCISSOR_TEST);
    gl.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    gl.UseProgram(this->blurProgram);
    gl.BindVertexArray(this->vertexArray);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, source);
    gl.Uniform1i(this->blur.source, 0);
    gl.Uniform2f(this->blur.inverseSourceResolution, 1.0f / (float)sourceWidth,
        1.0f / (float)sourceHeight);
    gl.Uniform2f(this->blur.direction, directionX, directionY);
    gl.Uniform1f(this->blur.centerWeight, plan.centerWeight);
    gl.Uniform1f(this->blur.pairCount, (float)plan.pairCount);
    gl.Uniform1f(this->blur.passKind, passKind);
    gl.Uniform1fv(this->blur.pairWeights, (GLsizei)kGlassGaussianPairCapacity,
        plan.pairWeights.data());
    gl.Uniform1fv(this->blur.pairOffsets, (GLsizei)kGlassGaussianPairCapacity,
        plan.pairOffsets.data());
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
}

bool GlGlassBackend::Impl::renderBlur(GLuint source, const GlassBlurPlan& plan)
{
    GlProcs& gl = glProcs();
    // 第一趟：全分辨率 → 半分辨率（2x2 box 降采样）
    this->renderBlurPass(source, this->pingFramebuffer, (uint32_t)this->width,
        (uint32_t)this->height, 0.0f, 0.0f, 0.0f, plan);
    // 第二趟：横向
    this->renderBlurPass(this->pingTexture, this->pongFramebuffer, this->blurWidth,
        this->blurHeight, 1.0f, 1.0f, 0.0f, plan);
    // 第三趟：纵向
    this->renderBlurPass(this->pongTexture, this->pingFramebuffer, this->blurWidth,
        this->blurHeight, 1.0f, 0.0f, 1.0f, plan);
    return gl.GetError() == GL_NO_ERROR;
}

GlGlassBackend::GlGlassBackend()
    : impl(new Impl())
{
}

GlGlassBackend::~GlGlassBackend()
{
    this->shutdown();
}

bool GlGlassBackend::ensure(int width, int height)
{
    if (width < 2 || height < 2)
        return false;

    Impl& self = *this->impl;
    if (self.disabled && self.failedWidth == width && self.failedHeight == height)
        return false;
    if (self.blurProgram != 0 && self.width == width && self.height == height)
        return true;
    if (self.blurProgram != 0)
        self.destroy();

    if (!loadGlProcs())
    {
        self.disabled = true;
        self.failedWidth = width;
        self.failedHeight = height;
        return false;
    }

    GlProcs& gl = glProcs();
    // 忽略宿主留下的 GL 错误：只有这一函数自己的失败才算失败
    while (gl.GetError() != GL_NO_ERROR)
    {
    }

    GLint previousDrawFramebuffer = 0;
    GLint previousReadFramebuffer = 0;
    gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
    gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);

    self.width      = width;
    self.height     = height;
    self.blurWidth  = (uint32_t)std::max(1, (width + 1) / 2);
    self.blurHeight = (uint32_t)std::max(1, (height + 1) / 2);

    self.blurProgram = linkProgram(shaders::kFullscreenVertex, shaders::kBlurFragment,
        "glass blur");
    self.glassProgram = linkProgram(shaders::kGlassVertex, shaders::kGlassFragment,
        "liquid glass");
    if (self.blurProgram == 0 || self.glassProgram == 0)
    {
        self.destroy();
        self.disabled = true;
        self.failedWidth = width;
        self.failedHeight = height;
        return false;
    }

    const bool targetsOk =
        createTarget("capture", (uint32_t)width, (uint32_t)height, self.captureTexture,
            self.captureFramebuffer)
        && createTarget("content-capture", (uint32_t)width, (uint32_t)height,
            self.contentCaptureTexture, self.contentCaptureFramebuffer)
        && createTarget("blur-ping", self.blurWidth, self.blurHeight, self.pingTexture,
            self.pingFramebuffer)
        && createTarget("blur-pong", self.blurWidth, self.blurHeight, self.pongTexture,
            self.pongFramebuffer);
    if (!targetsOk)
    {
        self.destroy();
        self.disabled = true;
        self.failedWidth = width;
        self.failedHeight = height;
        return false;
    }

    gl.GenVertexArrays(1, &self.vertexArray);

    self.blur.source = gl.GetUniformLocation(self.blurProgram, "uSource");
    self.blur.inverseSourceResolution
        = gl.GetUniformLocation(self.blurProgram, "uInverseSourceResolution");
    self.blur.direction = gl.GetUniformLocation(self.blurProgram, "uDirection");
    self.blur.centerWeight = gl.GetUniformLocation(self.blurProgram, "uCenterWeight");
    self.blur.pairCount = gl.GetUniformLocation(self.blurProgram, "uPairCount");
    self.blur.passKind = gl.GetUniformLocation(self.blurProgram, "uPassKind");
    self.blur.pairWeights = gl.GetUniformLocation(self.blurProgram, "uPairWeights");
    self.blur.pairOffsets = gl.GetUniformLocation(self.blurProgram, "uPairOffsets");

    self.glass.backdrop = gl.GetUniformLocation(self.glassProgram, "uBackdrop");
    self.glass.rawBackdrop = gl.GetUniformLocation(self.glassProgram, "uRawBackdrop");
    self.glass.bounds = gl.GetUniformLocation(self.glassProgram, "uBounds");
    self.glass.radii = gl.GetUniformLocation(self.glassProgram, "uRadii");
    self.glass.refraction = gl.GetUniformLocation(self.glassProgram, "uRefraction");
    self.glass.filter = gl.GetUniformLocation(self.glassProgram, "uFilter");
    self.glass.tint = gl.GetUniformLocation(self.glassProgram, "uTint");
    self.glass.framebuffer = gl.GetUniformLocation(self.glassProgram, "uFramebuffer");
    self.glass.deformation = gl.GetUniformLocation(self.glassProgram, "uDeformation");
    self.glass.lighting = gl.GetUniformLocation(self.glassProgram, "uLighting");
    self.glass.light = gl.GetUniformLocation(self.glassProgram, "uLight");
    self.glass.framebufferSize
        = gl.GetUniformLocation(self.glassProgram, "uFramebufferSize");

    self.prepared = false;
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)previousDrawFramebuffer);
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)previousReadFramebuffer);

    while (gl.GetError() != GL_NO_ERROR)
    {
    }
    BKLV_LOG_INFO("[玻璃] 后端就绪：%dx%d（模糊 %ux%u）", width, height, self.blurWidth,
        self.blurHeight);
    return true;
}

bool GlGlassBackend::isReady() const
{
    return this->impl->blurProgram != 0 && !this->impl->disabled;
}

EffectCapabilities GlGlassBackend::getCapabilities() const
{
    if (!this->isReady())
        return {};
    return EffectCapabilities{ true, true, true, true, kMaxGlassRegions };
}

bool GlGlassBackend::prepareBackdrop(const GlassRequestQueue& queue)
{
    Impl& self = *this->impl;
    if (!this->isReady() || !hasVisibleRequest(queue))
        return false;

    GlProcs& gl = glProcs();
    self.prepared = false;

    GLint sourceFramebuffer = 0;
    gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &sourceFramebuffer);
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)sourceFramebuffer);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, self.captureFramebuffer);
    gl.BlitFramebuffer(0, 0, self.width, self.height, 0, 0, self.width, self.height,
        GL_COLOR_BUFFER_BIT, GL_NEAREST);
    const float physicalRadius = clampFinite(
        queue.getBackdropStyle().blurRadius * queue.getDensity(), 0.0f, kMaximumBlurRadius,
        0.01f);
    const GlassBlurPlan plan
        = makeGlassBlurPlan(physicalRadius, queue.getBackdropStyle().quality);

    self.prepared = self.renderBlur(self.captureTexture, plan);
    self.activeSource = GlassBackdropSource::FRAME;

    return self.prepared;
}

void GlGlassBackend::composite(const GlassRequest& request)
{
    Impl& self = *this->impl;
    if (!this->isReady() || !self.prepared)
        return;

    const float width  = request.minMax.z - request.minMax.x;
    const float height = request.minMax.w - request.minMax.y;
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f || height <= 0.0f)
        return;

    GlProcs& gl = glProcs();
    const float density = std::max(request.density, 0.001f);
    const float shadowExtent = request.style.lighting.outerShadow > 0.0f
        ? clampFinite(request.style.lighting.shadowExtent * density, 0.0f, 32.0f * density,
              5.0f * density)
        : 0.0f;
    const float drawExtent = std::max(shadowExtent, 1.0f);

    const float clipMinX = std::max(
        std::max(0.0f, request.minMax.x - drawExtent), request.clipRect.x);
    const float clipMinY = std::max(
        std::max(0.0f, request.minMax.y - drawExtent), request.clipRect.y);
    const float clipMaxX = std::min(
        std::min((float)self.width, request.minMax.z + drawExtent), request.clipRect.z);
    const float clipMaxY = std::min(
        std::min((float)self.height, request.minMax.w + drawExtent), request.clipRect.w);
    if (clipMaxX <= clipMinX || clipMaxY <= clipMinY)
        return;

    gl.Viewport(0, 0, self.width, self.height);
    gl.Enable(GL_SCISSOR_TEST);
    gl.Scissor((GLint)std::floor(clipMinX),
        (GLint)std::floor((float)self.height - clipMaxY),
        (GLsizei)(std::ceil(clipMaxX) - std::floor(clipMinX)),
        (GLsizei)(std::ceil(clipMaxY) - std::floor(clipMinY)));
    gl.Enable(GL_BLEND);
    gl.BlendEquation(GL_FUNC_ADD);
    gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
        GL_ONE_MINUS_SRC_ALPHA);
    gl.Disable(GL_CULL_FACE);
    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_STENCIL_TEST);
    gl.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    gl.UseProgram(self.glassProgram);
    gl.BindVertexArray(self.vertexArray);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, self.pingTexture);
    gl.Uniform1i(self.glass.backdrop, 0);
    gl.ActiveTexture(GL_TEXTURE1);
    gl.BindTexture(GL_TEXTURE_2D,
        self.activeSource == GlassBackdropSource::PREVIOUS_CONTENT
            ? self.contentCaptureTexture
            : self.captureTexture);
    gl.Uniform1i(self.glass.rawBackdrop, 1);
    gl.Uniform4f(self.glass.bounds, request.minMax.x, request.minMax.y, request.minMax.z,
        request.minMax.w);

    const float maximumRadius = std::min(width, height) * 0.5f;
    float radii[4] = {
        clampFinite(request.radii.topLeft * density, 0.0f, maximumRadius, 0.0f),
        clampFinite(request.radii.topRight * density, 0.0f, maximumRadius, 0.0f),
        clampFinite(request.radii.bottomRight * density, 0.0f, maximumRadius, 0.0f),
        clampFinite(request.radii.bottomLeft * density, 0.0f, maximumRadius, 0.0f),
    };
    if (request.shape == GlassShapeKind::CAPSULE)
        std::fill(std::begin(radii), std::end(radii), maximumRadius);
    gl.Uniform4fv(self.glass.radii, 1, radii);

    gl.Uniform4f(self.glass.refraction,
        clampFinite(request.style.refractionHeight * density, 0.001f,
            std::max(0.001f, maximumRadius), 20.0f * density),
        clampFinite(request.style.refractionAmount * density, -960.0f, 960.0f,
            -70.0f * density),
        clampFinite(request.style.depthEffect, -2.0f, 2.0f, 0.3f),
        clampFinite(request.style.chromaticAberration, 0.0f, 1.0f, 0.5f));
    gl.Uniform4f(self.glass.filter,
        clampFinite(request.style.contrast, -1.0f, 1.0f, 0.0f),
        clampFinite(request.style.whitePoint, -1.0f, 1.0f, 0.0f),
        clampFinite(request.style.chromaMultiplier, 0.0f, 2.0f, 1.0f),
        clampFinite(request.opacity, 0.0f, 1.0f, 1.0f));
    gl.Uniform4f(self.glass.tint, clampFinite(request.style.tint.x, 0.0f, 1.0f, 1.0f),
        clampFinite(request.style.tint.y, 0.0f, 1.0f, 1.0f),
        clampFinite(request.style.tint.z, 0.0f, 1.0f, 1.0f),
        clampFinite(request.style.tint.w, 0.0f, 1.0f, 0.0f));
    gl.Uniform4f(self.glass.framebuffer, (float)self.width, (float)self.height, 0.0f,
        request.shape == GlassShapeKind::CIRCLE ? 1.0f : 0.0f);
    gl.Uniform4f(self.glass.deformation,
        clampFinite(request.deformation.x, -0.35f, 0.35f, 0.0f),
        clampFinite(request.deformation.y, -0.35f, 0.35f, 0.0f),
        clampFinite(request.blurMix, 0.0f, 1.0f, 1.0f), 0.0f);

    const GlassLighting& lighting = request.style.lighting;
    gl.Uniform4f(self.glass.lighting, clampFinite(lighting.innerShadow, 0.0f, 1.0f, 0.12f),
        clampFinite(lighting.rimGlow, 0.0f, 1.0f, 0.20f),
        clampFinite(lighting.specular, 0.0f, 2.0f, 0.35f),
        clampFinite(lighting.outerShadow, 0.0f, 1.0f, 0.14f));
    gl.Uniform4f(self.glass.light,
        clampFinite(lighting.lightDirection.x, -1.0f, 1.0f, -0.7071f),
        clampFinite(lighting.lightDirection.y, -1.0f, 1.0f, -0.7071f), shadowExtent,
        clampFinite(lighting.specularPower, 1.0f, 128.0f, 28.0f));
    gl.Uniform2f(self.glass.framebufferSize, (float)self.width, (float)self.height);

    gl.DrawArrays(GL_TRIANGLES, 0, 6);
    gl.ActiveTexture(GL_TEXTURE0);
}

void GlGlassBackend::finishFrame()
{
    this->impl->prepared = false;
}

void GlGlassBackend::shutdown()
{
    if (this->impl != nullptr && this->impl->blurProgram != 0)
        this->impl->destroy();
    if (this->impl != nullptr)
    {
        this->impl->disabled = false;
        this->impl->failedWidth = 0;
        this->impl->failedHeight = 0;
    }
}

namespace
{

struct CallbackData
{
    GlGlassBackend* backend = nullptr;
    const GlassRequest* request = nullptr;
    const GlassRequestQueue* queue = nullptr;
    bool* backdropPrepared = nullptr;
};

// 玻璃中间目标会改 GL 的 framebuffer 绑定（抓背景/模糊都画到离屏 FBO 上），
// 而 GL 的绑定是全局状态 —— 不改回来的话，**后面所有 ImGui 的命令都会画进离屏 FBO**，
// 屏幕上只剩"第一条玻璃命令之前"的内容（这次踩到：整屏四分之三变透明）。
// 所以这两件事做完必须把 read/draw framebuffer 恢复原样。
struct FramebufferGuard
{
    GLint draw = 0;
    GLint read = 0;

    FramebufferGuard()
    {
        GlProcs& gl = glProcs();
        gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &this->draw);
        gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &this->read);
    }

    ~FramebufferGuard()
    {
        GlProcs& gl = glProcs();
        gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)this->draw);
        gl.BindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)this->read);
    }
};

// ImGui 的绘制回调：进到这里时，帧缓冲里已经有"这条命令之前画好的一切"。
//
// 和源项目的差别：源项目是 Android 覆盖层，它在**画之前**抓帧缓冲
// （那是宿主 App 的画面，正是它要的背景）；设计器要的背景是"本帧已经画好的
// 壁纸 + 更早的面板"，所以抓取放在**第一条玻璃命令**里，一帧只抓一次、
// 当帧所有玻璃面共用。
void renderGlassCommand(const ImDrawList*, const ImDrawCmd* command)
{
    auto* data = static_cast<CallbackData*>(command->UserCallbackData);
    if (data == nullptr || data->backend == nullptr || data->request == nullptr)
        return;

    // 抓帧 + 模糊会改 framebuffer 绑定（都画到离屏 FBO），**必须在自己这一段里恢复** ——
    // 让守卫包住 composite 是错的：那样合成会被画进离屏 FBO、屏幕上什么都看不到
    //（FBO=3 留在绑定上，踩过）。
    if (data->backdropPrepared != nullptr && !*data->backdropPrepared
        && data->queue != nullptr)
    {
        const FramebufferGuard guard;
        *data->backdropPrepared = data->backend->prepareBackdrop(*data->queue);
    }
    data->backend->composite(*data->request);
}

} // namespace

void GlGlassBackend::inject(ImDrawData& drawData, const GlassRequestQueue& queue)
{
    if (!this->isReady() || queue.isEmpty())
        return;

    const std::vector<GlassRequest>& requests = queue.getRequests();

    // 回调数据要活到渲染结束：放在函数级静态里（一帧一次，下一帧覆盖）
    static bool backdropPrepared = false;
    static std::vector<CallbackData> callbackData;
    backdropPrepared = false;
    callbackData.clear();
    callbackData.reserve(requests.size());
    for (const GlassRequest& request : requests)
        callbackData.push_back(CallbackData{ this, &request, &queue, &backdropPrepared });

    // 每个 draw list 里插过的命令数（同一条命令之后插 N 次要依次错开）
    struct Injection
    {
        ImDrawList* drawList = nullptr;
        std::vector<int> commandOffsets;
    };
    static std::vector<Injection> injections;
    injections.clear();

    size_t index = 0;
    for (const GlassRequest& request : requests)
    {
        index++;
        if (request.drawList == nullptr)
            continue;

        // 这块 draw list 属于本次绘制数据吗？（不属于就跳过，免得插到别人身上）
        bool belongs = false;
        for (int i = 0; i < drawData.CmdListsCount; i++)
            if (drawData.CmdLists[i] == request.drawList)
                belongs = true;
        if (!belongs)
            continue;

        auto injection = std::find_if(injections.begin(), injections.end(),
            [&request](const Injection& value) { return value.drawList == request.drawList; });
        if (injection == injections.end())
        {
            injections.push_back(Injection{ request.drawList, {} });
            injection = std::prev(injections.end());
        }

        ImDrawCmd glassCommand;
        glassCommand.ClipRect = request.clipRect;
        glassCommand.UserCallback = renderGlassCommand;
        glassCommand.UserCallbackData = (void*)&callbackData[index - 1];
        glassCommand.UserCallbackDataOffset = -1;

        const int offset = std::min(std::max(request.drawCommandOffset, 0),
            request.drawList->CmdBuffer.Size);
        const int preceding = (int)std::count_if(injection->commandOffsets.begin(),
            injection->commandOffsets.end(), [offset](int value) { return value <= offset; });
        const int insertion = offset + preceding * 2;

        request.drawList->CmdBuffer.insert(
            request.drawList->CmdBuffer.begin() + insertion, glassCommand);

        // 玻璃改了 GL 状态，插一条"重置渲染状态"让 ImGui 接着画自己的东西
        ImDrawCmd resetCommand;
        resetCommand.ClipRect = request.clipRect;
        resetCommand.UserCallback = ImGui::GetPlatformIO().DrawCallback_ResetRenderState;
        resetCommand.UserCallbackDataOffset = -1;
        request.drawList->CmdBuffer.insert(
            request.drawList->CmdBuffer.begin() + insertion + 1, resetCommand);

        injection->commandOffsets.push_back(offset);
    }
}

} // namespace bklv::glass

#else // 非 GL 构建：桩实现（能力为空，服务层据此整层关闭）

namespace bklv::glass
{

struct GlGlassBackend::Impl
{
};

GlGlassBackend::GlGlassBackend() = default;
GlGlassBackend::~GlGlassBackend() = default;
bool GlGlassBackend::ensure(int, int) { return false; }
bool GlGlassBackend::isReady() const { return false; }
EffectCapabilities GlGlassBackend::getCapabilities() const { return EffectCapabilities{}; }
bool GlGlassBackend::prepareBackdrop(const GlassRequestQueue&) { return false; }
void GlGlassBackend::composite(const GlassRequest&) { }
void GlGlassBackend::finishFrame() { }
void GlGlassBackend::shutdown() { }
void GlGlassBackend::inject(ImDrawData&, const GlassRequestQueue&) { }

} // namespace bklv::glass

#endif // BKLV_OPENGL
