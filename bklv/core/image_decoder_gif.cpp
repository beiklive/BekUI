// GIF 解码（静态首帧 + 逐帧动画）。
//
// 为什么单独一个编译单元：这里要**独立实例化一份 stb_image**，只带 GIF，并且用
// `STB_IMAGE_STATIC` 把所有符号变成文件内静态 —— 否则会和 image_decoder.cpp 里
// 那份 stb（PNG/JPEG/BMP）撞符号。
//
// 为什么用 stb 的 GIF 而不是自己写 LZW：`stbi_load_gif_from_memory()` 返回的是
// **已经合成好的整幅画布帧**（帧内局部矩形、透明、disposal 都在 stb 里处理过），
// 外加每帧延迟（毫秒）。等于把 GIF 播放里最难的一块（帧合成）直接给了，
// 而且所有平台同一份实现 —— 桌面和 Switch 不会有行为差异。
//
// 许可：stb_image 是 public domain / MIT，已经在 bklv/3rd/stb 里，不新增依赖。

// 第三方单头文件的警告只在这一个编译单元里压掉（和 image_writer.cpp 同一约定）：
// STB_IMAGE_STATIC 会把没用到的函数全变成 static，必然报 -Wunused-function。
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STBI_NO_STDIO_WRITE

#include <stb_image.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <string>

#include "bklv/core/image_decoder.hpp"

namespace bklv
{
namespace detail
{

namespace
{

// GIF 的帧延迟单位是 1/100 秒，很多图写 0 或 1（浏览器一律兜到 100ms），
// 不兜的话播放会变成"一帧一闪"甚至 fmod 除零。
constexpr float kDefaultFrameDelay = 0.1f;

} // namespace

DecodedAnimation decodeGifAnimation(const unsigned char* data, size_t size)
{
    DecodedAnimation result;
    if (data == nullptr || size == 0)
    {
        result.error = "GIF 数据为空";
        return result;
    }

    const ImageLimits limits = getImageLimits();

    int* delays      = nullptr;
    int width        = 0;
    int height       = 0;
    int frameCount   = 0;
    int channels     = 0;

    // req_comp = 4：直接要 RGBA（非预乘，stb 给的就是直通 alpha）
    unsigned char* pixels = stbi_load_gif_from_memory(data, (int)size, &delays, &width, &height,
        &frameCount, &channels, 4);

    if (pixels == nullptr || width <= 0 || height <= 0 || frameCount <= 0)
    {
        const char* reason = stbi_failure_reason();
        result.error = std::string("GIF 解码失败") + (reason != nullptr ? std::string("：") + reason : "");
        if (pixels != nullptr)
            stbi_image_free(pixels);
        if (delays != nullptr)
            stbi_image_free(delays);
        return result;
    }

    // 卡上限：单帧尺寸、帧数、以及"所有帧加起来的像素"
    std::string limitError;
    if (!isImageSizeAllowed(width, height, &limitError))
    {
        stbi_image_free(pixels);
        stbi_image_free(delays);
        result.error = limitError;
        return result;
    }

    if ((size_t)frameCount > limits.maxAnimationFrames)
    {
        stbi_image_free(pixels);
        stbi_image_free(delays);
        result.error = "GIF 帧数超限：" + std::to_string(frameCount) + " 帧（上限 "
            + std::to_string(limits.maxAnimationFrames) + "）";
        return result;
    }

    const size_t framePixels = (size_t)width * (size_t)height;
    if (framePixels * (size_t)frameCount > limits.maxAnimationPixels)
    {
        stbi_image_free(pixels);
        stbi_image_free(delays);
        result.error = "GIF 总像素超限：" + std::to_string(width) + "x" + std::to_string(height)
            + " × " + std::to_string(frameCount) + " 帧（上限 "
            + std::to_string(limits.maxAnimationPixels) + " 像素）";
        return result;
    }

    const size_t frameBytes = framePixels * 4;
    result.width  = width;
    result.height = height;
    result.frames.reserve((size_t)frameCount);
    result.delays.reserve((size_t)frameCount);

    for (int i = 0; i < frameCount; i++)
    {
        const unsigned char* begin = pixels + frameBytes * (size_t)i;
        result.frames.emplace_back(begin, begin + frameBytes);

        // stb 给的延迟是毫秒（源码里 delay = 10 * 1/100 秒）；0/1 兜到 100ms
        const float seconds = delays != nullptr ? (float)delays[i] / 1000.0f : 0.0f;
        result.delays.push_back(seconds > 0.02f ? seconds : kDefaultFrameDelay);
    }

    stbi_image_free(pixels);
    if (delays != nullptr)
        stbi_image_free(delays);

    // GIF 的循环次数（NETSCAPE2.0 扩展）stb 没有暴露出来，统一按"循环播放"处理
    result.loop = true;
    return result;
}

} // namespace detail
} // namespace bklv
