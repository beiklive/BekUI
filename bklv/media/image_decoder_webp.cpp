// WebP 解码（静态 + 逐帧动画）。libwebp 是可选依赖（CMake 的 BKLV_WITH_WEBP）。
//
// 和 GIF 一样挂在同一条链路上：静态图 → DecodedImage，动图 → DecodedAnimation，
// 播放交给 views/image.* 的帧时钟（不额外写一套播放器）。
//
// 为什么用 WebPAnimDecoder（webpdemux）而不是自己拼帧：
// 它已经处理了画布尺寸、帧内偏移、透明与 disposal（每个 WebP 帧只存变化区域），
// 拿到的就是**合成好的整幅画布**。帧延迟用它给出的时间戳差值换算。
//
// libwebp 解码出来的是**非预乘** RGBA（正好符合本项目"只准非预乘"的契约）。

#include <string>

#include "bklv/media/image_decoder.hpp"

#if defined(BKLV_USE_WEBP)

#include <webp/decode.h>
#include <webp/demux.h>

namespace bklv
{
namespace detail
{

namespace
{

// 动图上限（和 GIF 同一套限制）
bool checkAnimationBudget(int width, int height, size_t frames, std::string& error)
{
    const ImageLimits limits = getImageLimits();

    if (!isImageSizeAllowed(width, height, &error))
        return false;

    if (frames > limits.maxAnimationFrames)
    {
        error = "WebP 帧数超限：" + std::to_string(frames) + " 帧（上限 "
            + std::to_string(limits.maxAnimationFrames) + "）";
        return false;
    }

    if ((size_t)width * (size_t)height * frames > limits.maxAnimationPixels)
    {
        error = "WebP 总像素超限：" + std::to_string(width) + "x" + std::to_string(height) + " × "
            + std::to_string(frames) + " 帧（上限 "
            + std::to_string(limits.maxAnimationPixels) + " 像素）";
        return false;
    }

    return true;
}

} // namespace

bool isWebPSupported()
{
    return true;
}

DecodedImage decodeWebPImage(const unsigned char* data, size_t size)
{
    DecodedImage result;
    if (data == nullptr || size == 0)
    {
        result.error = "WebP 数据为空";
        return result;
    }

    int width  = 0;
    int height = 0;
    if (WebPGetInfo(data, size, &width, &height) == 0)
    {
        result.error = "WebP 头解析失败（文件可能损坏，或不是 WebP）";
        return result;
    }

    // 先卡上限，再解码（WebP 头里就有画布尺寸）
    if (!isImageSizeAllowed(width, height, &result.error))
        return result;

    uint8_t* pixels = WebPDecodeRGBA(data, size, &width, &height);
    if (pixels == nullptr)
    {
        result.error = "WebP 解码失败";
        return result;
    }

    const size_t byteCount = (size_t)width * (size_t)height * 4;
    result.pixels.assign(pixels, pixels + byteCount);
    result.width  = width;
    result.height = height;
    result.stride = width * 4;

    WebPFree(pixels);
    return result;
}

DecodedAnimation decodeWebPAnimation(const unsigned char* data, size_t size)
{
    DecodedAnimation result;
    if (data == nullptr || size == 0)
    {
        result.error = "WebP 数据为空";
        return result;
    }

    // 没有动画的 WebP：给一帧，交给静态解码（避免调用方多写一次判断）
    WebPAnimDecoderOptions options;
    if (WebPAnimDecoderOptionsInit(&options) == 0)
    {
        result.error = "WebP 动画：初始化选项失败";
        return result;
    }
    options.color_mode = MODE_RGBA;
    options.use_threads = 0; // 解码在主线程/工作线程已由调用方决定，这里不再开线程

    WebPData webpData = { data, size };
    WebPAnimDecoder* decoder = WebPAnimDecoderNew(&webpData, &options);
    if (decoder == nullptr)
    {
        result.error = "WebP 动画解码器创建失败";
        return result;
    }

    WebPAnimInfo info;
    if (WebPAnimDecoderGetInfo(decoder, &info) == 0 || info.frame_count == 0)
    {
        WebPAnimDecoderDelete(decoder);
        result.error = "WebP 动画信息读取失败";
        return result;
    }

    if (!checkAnimationBudget((int)info.canvas_width, (int)info.canvas_height, info.frame_count,
            result.error))
    {
        WebPAnimDecoderDelete(decoder);
        return result;
    }

    result.width  = (int)info.canvas_width;
    result.height = (int)info.canvas_height;
    result.loop   = info.loop_count == 0; // 0 = 无限循环

    const size_t frameBytes = (size_t)result.width * (size_t)result.height * 4;
    result.frames.reserve(info.frame_count);
    result.delays.reserve(info.frame_count);

    uint8_t* framePixels = nullptr;
    int timestamp        = 0;
    int previousStamp    = 0;

    while (WebPAnimDecoderGetNext(decoder, &framePixels, &timestamp) != 0)
    {
        if (framePixels == nullptr)
            break;

        result.frames.emplace_back(framePixels, framePixels + frameBytes);

        // 时间戳是"这一帧开始显示的时刻"（毫秒），延迟 = 与上一帧的差值；
        // 第一帧的延迟要等第二帧才知道，所以先占位，循环结束后回填。
        const int delta = timestamp - previousStamp;
        result.delays.push_back(delta > 0 ? (float)delta / 1000.0f : 0.0f);
        previousStamp = timestamp;
    }

    WebPAnimDecoderDelete(decoder);

    if (result.frames.empty())
    {
        result.error = "WebP 动画没有解出任何帧";
        return result;
    }

    // 首帧延迟由第二帧的时间戳回填（只有一帧时用默认 100ms）；
    // 最后一帧沿用前一帧的延迟（时间戳里没有它的结束时刻）。
    if (result.delays.size() > 1)
        result.delays[0] = result.delays[1];
    if (result.delays.front() <= 0.0f)
        result.delays.front() = 0.1f;
    for (float& delay : result.delays)
    {
        if (delay <= 0.02f)
            delay = 0.1f; // 和 GIF 一样兜底：0/极小延迟按 100ms
    }

    return result;
}

} // namespace detail
} // namespace bklv

#else // !BKLV_USE_WEBP

namespace bklv
{
namespace detail
{

bool isWebPSupported()
{
    return false;
}

DecodedImage decodeWebPImage(const unsigned char*, size_t)
{
    DecodedImage result;
    result.error = "这个构建没有编 WebP 解码（CMake 的 BKLV_WITH_WEBP=OFF，或 bklv/3rd/webp 子模块缺失）";
    return result;
}

DecodedAnimation decodeWebPAnimation(const unsigned char*, size_t)
{
    DecodedAnimation result;
    result.error = "这个构建没有编 WebP 解码（CMake 的 BKLV_WITH_WEBP=OFF，或 bklv/3rd/webp 子模块缺失）";
    return result;
}

} // namespace detail
} // namespace bklv

#endif
