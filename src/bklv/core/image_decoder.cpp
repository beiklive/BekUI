#include "bklv/core/image_decoder.hpp"

// ---------------------------------------------------------------------------
// 各平台的解码实现
//
// 优先用系统解码器（macOS 的 ImageIO 覆盖格式最全、体积为零），
// 其余平台统一用 stb_image。两者都不可用时返回空图，上层显示占位色块。
// ---------------------------------------------------------------------------

#if defined(__APPLE__) && !defined(BKLV_FORCE_STB_IMAGE)

// ============================ macOS: ImageIO ============================

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <climits>
#include <cstdlib>

#include "bklv/core/logger.hpp"

namespace bklv
{

namespace
{

DecodedImage decodeWithSource(CGImageSourceRef source)
{
    DecodedImage result;
    if (source == nullptr)
        return result;

    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    if (image == nullptr)
        return result;

    const size_t width  = CGImageGetWidth(image);
    const size_t height = CGImageGetHeight(image);

    if (width == 0 || height == 0)
    {
        CGImageRelease(image);
        return result;
    }

    result.pixels.assign(width * height * 4, 0);
    result.width  = (int)width;
    result.height = (int)height;

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    // kCGImageAlphaPremultipliedLast | ByteOrder32Big => RGBA 顺序
    CGBitmapInfo bitmapInfo = kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big;
    CGContextRef context =
        CGBitmapContextCreate(result.pixels.data(), width, height, 8, width * 4, colorSpace, bitmapInfo);

    if (context != nullptr)
    {
        CGContextDrawImage(context, CGRectMake(0, 0, (CGFloat)width, (CGFloat)height), image);
        CGContextRelease(context);
    }
    else
    {
        result.pixels.clear();
        result.width  = 0;
        result.height = 0;
    }

    CGColorSpaceRelease(colorSpace);
    CGImageRelease(image);
    return result;
}

} // namespace

const char* getImageDecoderName()
{
    return "ImageIO";
}

DecodedImage decodeImageFile(const char* path)
{
    if (path == nullptr)
        return DecodedImage();

    // CFURLCreateWithFileSystemPath 不解析相对路径，先转绝对路径。
    char absolute[PATH_MAX] = { 0 };
    const char* resolved    = realpath(path, absolute);
    const std::string fullPath = resolved != nullptr ? std::string(resolved) : std::string(path);

    DecodedImage result;

    CFStringRef cfPath = CFStringCreateWithCString(nullptr, fullPath.c_str(), kCFStringEncodingUTF8);
    if (cfPath == nullptr)
        return result;

    CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, cfPath, kCFURLPOSIXPathStyle, false);
    if (url != nullptr)
    {
        CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
        result = decodeWithSource(source);
        if (source != nullptr)
            CFRelease(source);
        CFRelease(url);
    }

    CFRelease(cfPath);
    return result;
}

DecodedImage decodeImageMemory(const void* data, size_t size)
{
    if (data == nullptr || size == 0)
        return DecodedImage();

    CFDataRef cfData = CFDataCreate(nullptr, static_cast<const UInt8*>(data), (CFIndex)size);
    if (cfData == nullptr)
        return DecodedImage();

    CGImageSourceRef source = CGImageSourceCreateWithData(cfData, nullptr);
    DecodedImage result = decodeWithSource(source);

    if (source != nullptr)
        CFRelease(source);
    CFRelease(cfData);
    return result;
}

} // namespace bklv

#elif defined(BKLV_USE_STB_IMAGE)

// ===================== Linux / Switch / 其他: stb_image =====================

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_NO_STDIO_WRITE

#include <stb_image.h>

namespace bklv
{

namespace
{

// stb 返回的是 RGB 或 RGBA，统一转成 RGBA8。
DecodedImage toRGBA(unsigned char* data, int width, int height, int channels)
{
    DecodedImage result;
    if (data == nullptr || width <= 0 || height <= 0)
        return result;

    const size_t pixelCount = (size_t)width * (size_t)height;
    result.pixels.resize(pixelCount * 4);
    result.width  = width;
    result.height = height;

    for (size_t i = 0; i < pixelCount; ++i)
    {
        const unsigned char* src = data + i * (size_t)channels;
        unsigned char* dst       = result.pixels.data() + i * 4;

        if (channels >= 3)
        {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = channels == 4 ? src[3] : 255;
        }
        else
        {
            // 单通道灰度图
            dst[0] = dst[1] = dst[2] = src[0];
            dst[3]                   = 255;
        }
    }

    stbi_image_free(data);
    return result;
}

} // namespace

const char* getImageDecoderName()
{
    return "stb_image";
}

DecodedImage decodeImageFile(const char* path)
{
    if (path == nullptr)
        return DecodedImage();

    int width = 0, height = 0, channels = 0;
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
    return toRGBA(data, width, height, channels);
}

DecodedImage decodeImageMemory(const void* data, size_t size)
{
    if (data == nullptr || size == 0)
        return DecodedImage();

    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(static_cast<const stbi_uc*>(data), (int)size, &width,
        &height, &channels, 0);
    return toRGBA(pixels, width, height, channels);
}

} // namespace bklv

#else

// ============================ 无解码器 ============================

namespace bklv
{

const char* getImageDecoderName()
{
    return "none";
}

DecodedImage decodeImageFile(const char*)
{
    return DecodedImage();
}

DecodedImage decodeImageMemory(const void*, size_t)
{
    return DecodedImage();
}

} // namespace bklv

#endif
