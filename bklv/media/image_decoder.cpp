#include "bklv/media/image_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "bklv/core/logger.hpp"

// ---------------------------------------------------------------------------
// 结构：
//   ① 平台无关部分：按内容认格式、读头部拿尺寸、卡上限、对外统一分发
//   ② 平台解码器：只解 PNG/JPEG/BMP（macOS = ImageIO，其余 = stb_image）
//   ③ GIF：所有平台都走 image_decoder_gif.cpp（自带的 stb GIF 解码器）
//
// "先认格式再分发"是刻意的：解码器不再各自处理所有格式，换/加解码器只改一处；
// 同一张图在不同平台上也会走同一条路（GIF 尤其重要，否则桌面和 Switch 表现不一致）。
// ---------------------------------------------------------------------------

namespace bklv
{

// 各格式解码器（各自一个编译单元）：文件内使用
namespace detail
{
DecodedAnimation decodeGifAnimation(const unsigned char* data, size_t size);
DecodedImage decodeWebPImage(const unsigned char* data, size_t size);
DecodedAnimation decodeWebPAnimation(const unsigned char* data, size_t size);
DecodedImage decodeSvgMemory(const unsigned char* data, size_t size, int width, int height);
// 可选解码器是否编进来了（对应 TU 里按宏给出真/假）
bool isWebPSupported();
bool isSvgSupported();
}

namespace
{

// ---------------- 按内容认格式 ----------------

bool startsWith(const unsigned char* bytes, size_t size, const char* signature, size_t length,
    size_t offset = 0)
{
    if (bytes == nullptr || size < offset + length)
        return false;
    return std::memcmp(bytes + offset, signature, length) == 0;
}

unsigned int readU16le(const unsigned char* p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

unsigned int readU16be(const unsigned char* p)
{
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

unsigned int readU32be(const unsigned char* p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8)
        | (unsigned int)p[3];
}

// PNG: 8 字节魔数 + IHDR 宽高（大端）
bool probePng(const unsigned char* p, size_t size, int& width, int& height)
{
    if (size < 24 || !startsWith(p, size, "\x89PNG\r\n\x1a\n", 8))
        return false;

    width  = (int)readU32be(p + 16);
    height = (int)readU32be(p + 20);
    return width > 0 && height > 0;
}

// BMP: "BM" + DIB 头宽高（小端；高度为负表示自上而下）
bool probeBmp(const unsigned char* p, size_t size, int& width, int& height)
{
    if (size < 26 || !startsWith(p, size, "BM", 2))
        return false;

    const int w = (int)(int32_t)((unsigned int)p[18] | ((unsigned int)p[19] << 8)
        | ((unsigned int)p[20] << 16) | ((unsigned int)p[21] << 24));
    const int h = (int)(int32_t)((unsigned int)p[22] | ((unsigned int)p[23] << 8)
        | ((unsigned int)p[24] << 16) | ((unsigned int)p[25] << 24));

    width  = std::abs(w);
    height = std::abs(h);
    return width > 0 && height > 0;
}

// GIF: "GIF87a"/"GIF89a" + 逻辑屏宽高（小端）
bool probeGif(const unsigned char* p, size_t size, int& width, int& height)
{
    if (size < 10 || !(startsWith(p, size, "GIF87a", 6) || startsWith(p, size, "GIF89a", 6)))
        return false;

    width  = (int)readU16le(p + 6);
    height = (int)readU16le(p + 8);
    return width > 0 && height > 0;
}

// JPEG: 扫 SOFn 段找宽高（没有全局头，只能走段链）
bool probeJpeg(const unsigned char* p, size_t size, int& width, int& height)
{
    if (size < 4 || p[0] != 0xFF || p[1] != 0xD8)
        return false;

    size_t offset = 2;
    while (offset + 9 < size)
    {
        if (p[offset] != 0xFF)
        {
            offset++;
            continue;
        }

        const unsigned char marker = p[offset + 1];
        offset += 2;

        // 无参数段
        if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7))
            continue;
        if (marker == 0xD9 || marker == 0xDA)
            break; // EOI / SOS：后面没有尺寸信息了

        if (offset + 2 > size)
            break;

        const size_t segmentLength = readU16be(p + offset);
        if (segmentLength < 2 || offset + segmentLength > size)
            break;

        // SOF0..SOF3 / SOF5..SOF7 / SOF9..SOF11 / SOF13..SOF15（不含 DHT/DAC 等）
        const bool isSof = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8
            && marker != 0xCC;
        if (isSof && segmentLength >= 7)
        {
            height = (int)readU16be(p + offset + 3);
            width  = (int)readU16be(p + offset + 5);
            return width > 0 && height > 0;
        }

        offset += segmentLength;
    }

    return false;
}

} // namespace

ImageFormat detectImageFormat(const void* data, size_t size)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    if (bytes == nullptr || size < 4)
        return ImageFormat::UNKNOWN;

    if (startsWith(bytes, size, "\x89PNG\r\n\x1a\n", 8))
        return ImageFormat::PNG;
    if (startsWith(bytes, size, "\xFF\xD8\xFF", 3))
        return ImageFormat::JPEG;
    if (startsWith(bytes, size, "BM", 2))
        return ImageFormat::BMP;
    if (startsWith(bytes, size, "GIF87a", 6) || startsWith(bytes, size, "GIF89a", 6))
        return ImageFormat::GIF;
    if (startsWith(bytes, size, "RIFF", 4) && startsWith(bytes, size, "WEBP", 4, 8))
        return ImageFormat::WEBP;
    if (startsWith(bytes, size, "II*\0", 4) || startsWith(bytes, size, "MM\0*", 4))
        return ImageFormat::TIFF;
    if (startsWith(bytes, size, "\0\0\x01\0", 4))
        return ImageFormat::ICO;

    // SVG 是文本：允许前导空白与 UTF-8 BOM，认 "<?xml" 或 "<svg"
    {
        size_t offset = 0;
        if (size >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
            offset = 3;
        while (offset < size
            && (bytes[offset] == ' ' || bytes[offset] == '\t' || bytes[offset] == '\r'
                || bytes[offset] == '\n'))
            offset++;

        if (size - offset >= 5
            && (std::memcmp(bytes + offset, "<?xml", 5) == 0
                || std::memcmp(bytes + offset, "<svg ", 5) == 0
                || (size - offset >= 4 && std::memcmp(bytes + offset, "<svg", 4) == 0)))
            return ImageFormat::SVG;
    }

    // HEIF/HEIC/AVIF：ISO-BMFF，第 4 字节起是 ftyp box，品牌决定具体格式
    if (startsWith(bytes, size, "ftyp", 4, 4) && size >= 12)
    {
        const char* brand = reinterpret_cast<const char*>(bytes + 8);
        const char* known[] = { "heic", "heix", "hevc", "heim", "heis", "hevm", "hevs", "mif1",
            "msf1", "avif", "avis" };
        for (const char* candidate : known)
        {
            if (std::memcmp(brand, candidate, 4) == 0)
                return ImageFormat::HEIC;
        }
    }

    return ImageFormat::UNKNOWN;
}

bool isImageFormatSupported(ImageFormat format)
{
    switch (format)
    {
        case ImageFormat::PNG:
        case ImageFormat::JPEG:
        case ImageFormat::BMP:
            // 平台解码器永远在（macOS 的 ImageIO / 其余平台的 stb）
            return true;
        case ImageFormat::GIF:
            // GIF 解码器（stb）永远编进来
            return true;
        case ImageFormat::WEBP:
            return detail::isWebPSupported();
        case ImageFormat::SVG:
            return detail::isSvgSupported();
        case ImageFormat::TIFF:
        case ImageFormat::HEIC:
        case ImageFormat::ICO:
            // 只有 macOS 的 ImageIO 认这些
#if defined(__APPLE__) && !defined(BKLV_FORCE_STB_IMAGE)
            return true;
#else
            return false;
#endif
        case ImageFormat::UNKNOWN:
            break;
    }
    return false;
}

const char* getImageFormatName(ImageFormat format)
{
    switch (format)
    {
        case ImageFormat::PNG: return "PNG";
        case ImageFormat::JPEG: return "JPEG";
        case ImageFormat::BMP: return "BMP";
        case ImageFormat::GIF: return "GIF";
        case ImageFormat::WEBP: return "WebP";
        case ImageFormat::SVG: return "SVG";
        case ImageFormat::TIFF: return "TIFF";
        case ImageFormat::HEIC: return "HEIC/AVIF";
        case ImageFormat::ICO: return "ICO";
        case ImageFormat::UNKNOWN: break;
    }
    return "未知格式";
}

ImageLimits getImageLimits()
{
    return ImageLimits();
}

bool probeImageSize(const void* data, size_t size, int& width, int& height)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    if (bytes == nullptr)
        return false;

    switch (detectImageFormat(data, size))
    {
        case ImageFormat::PNG: return probePng(bytes, size, width, height);
        case ImageFormat::JPEG: return probeJpeg(bytes, size, width, height);
        case ImageFormat::BMP: return probeBmp(bytes, size, width, height);
        case ImageFormat::GIF: return probeGif(bytes, size, width, height);
        default: break;
    }
    return false;
}

bool isImageSizeAllowed(int width, int height, std::string* error)
{
    const ImageLimits limits = getImageLimits();

    if (width <= 0 || height <= 0)
    {
        if (error != nullptr)
            *error = "图片尺寸无效";
        return false;
    }

    if (width > limits.maxDimension || height > limits.maxDimension)
    {
        if (error != nullptr)
        {
            *error = "图片尺寸超限：" + std::to_string(width) + "x" + std::to_string(height)
                + "（单边上限 " + std::to_string(limits.maxDimension) + "）";
        }
        return false;
    }

    const size_t pixels = (size_t)width * (size_t)height;
    if (pixels > limits.maxPixels)
    {
        if (error != nullptr)
        {
            *error = "图片像素超限：" + std::to_string(width) + "x" + std::to_string(height)
                + "（上限 " + std::to_string(limits.maxPixels) + " 像素）";
        }
        return false;
    }

    return true;
}

float DecodedAnimation::getDuration() const
{
    float total = 0.0f;
    for (float delay : this->delays)
        total += delay;
    return total;
}

size_t frameIndexAt(const DecodedAnimation& animation, float elapsedSeconds)
{
    const size_t count = animation.frames.size();
    if (count == 0)
        return 0;
    if (count == 1 || elapsedSeconds <= 0.0f)
        return 0;

    float total = animation.getDuration();
    if (total <= 0.0f)
        return 0;

    float time = std::fmod(elapsedSeconds, total);
    if (time < 0.0f)
        time += total;

    for (size_t i = 0; i < count; i++)
    {
        const float delay = animation.delays[i];
        if (time < delay)
            return i;
        time -= delay;
    }

    return count - 1;
}

// ---------------------------------------------------------------------------
// 平台解码器：只解 PNG / JPEG / BMP（GIF 走自带 GIF 解码器）
// ---------------------------------------------------------------------------

#if defined(__APPLE__) && !defined(BKLV_FORCE_STB_IMAGE)

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

namespace
{

// macOS 的 CGBitmapContext **只支持预乘 alpha**（`kCGImageAlphaLast` 不被接受），
// 而 ImGui 的混合方程是直通 alpha → 预乘图整体偏暗。所以解完做一次反预乘。
void unpremultiplyRgba(unsigned char* pixels, size_t pixelCount)
{
    for (size_t i = 0; i < pixelCount; i++)
    {
        unsigned char* px = pixels + i * 4;
        const unsigned int alpha = px[3];
        if (alpha == 255)
            continue;
        if (alpha == 0)
        {
            px[0] = 0;
            px[1] = 0;
            px[2] = 0;
            continue;
        }

        for (int c = 0; c < 3; c++)
        {
            const unsigned int value = ((unsigned int)px[c] * 255u + alpha / 2u) / alpha;
            px[c] = (unsigned char)std::min(value, 255u);
        }
    }
}

DecodedImage decodePlatformImage(const unsigned char* data, size_t size, const char* /*path*/)
{
    DecodedImage result;

    CFDataRef cfData = CFDataCreate(nullptr, static_cast<const UInt8*>(data), (CFIndex)size);
    if (cfData == nullptr)
    {
        result.error = "ImageIO：创建数据对象失败";
        return result;
    }

    CGImageSourceRef source = CGImageSourceCreateWithData(cfData, nullptr);
    if (source == nullptr)
    {
        CFRelease(cfData);
        result.error = "ImageIO 不认这段数据";
        return result;
    }

    // 先看属性里的尺寸，超限就别解码了（避免先分配再拒绝）
    int declaredWidth  = 0;
    int declaredHeight = 0;
    CFDictionaryRef properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nullptr);
    if (properties != nullptr)
    {
        CFNumberRef widthNumber = (CFNumberRef)CFDictionaryGetValue(
            properties, kCGImagePropertyPixelWidth);
        CFNumberRef heightNumber = (CFNumberRef)CFDictionaryGetValue(
            properties, kCGImagePropertyPixelHeight);
        if (widthNumber != nullptr)
            CFNumberGetValue(widthNumber, kCFNumberIntType, &declaredWidth);
        if (heightNumber != nullptr)
            CFNumberGetValue(heightNumber, kCFNumberIntType, &declaredHeight);
        CFRelease(properties);
    }

    if (declaredWidth > 0 && declaredHeight > 0
        && !isImageSizeAllowed(declaredWidth, declaredHeight, &result.error))
    {
        CFRelease(source);
        CFRelease(cfData);
        return result;
    }

    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    if (image == nullptr)
    {
        CFRelease(source);
        CFRelease(cfData);
        result.error = "ImageIO 解码失败";
        return result;
    }

    const size_t width  = CGImageGetWidth(image);
    const size_t height = CGImageGetHeight(image);
    if (width == 0 || height == 0
        || !isImageSizeAllowed((int)width, (int)height, &result.error))
    {
        CGImageRelease(image);
        CFRelease(source);
        CFRelease(cfData);
        if (result.error.empty())
            result.error = "ImageIO：图片尺寸为 0";
        return result;
    }

    result.pixels.assign(width * height * 4, 0);
    result.width  = (int)width;
    result.height = (int)height;
    result.stride = (int)width * 4;

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    // kCGImageAlphaPremultipliedLast | ByteOrder32Big => RGBA 顺序（解码后反预乘）
    CGBitmapInfo bitmapInfo = kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big;
    CGContextRef context    = CGBitmapContextCreate(result.pixels.data(), width, height, 8,
        width * 4, colorSpace, bitmapInfo);

    if (context != nullptr)
    {
        CGContextDrawImage(context, CGRectMake(0, 0, (CGFloat)width, (CGFloat)height), image);
        CGContextRelease(context);

        const CGImageAlphaInfo alphaInfo = CGImageGetAlphaInfo(image);
        const bool hasAlpha = alphaInfo != kCGImageAlphaNone && alphaInfo != kCGImageAlphaNoneSkipFirst
            && alphaInfo != kCGImageAlphaNoneSkipLast;
        if (hasAlpha)
            unpremultiplyRgba(result.pixels.data(), width * height);
    }
    else
    {
        result.pixels.clear();
        result.width  = 0;
        result.height = 0;
        result.stride = 0;
        result.error  = "ImageIO：创建位图上下文失败";
    }

    CGColorSpaceRelease(colorSpace);
    CGImageRelease(image);
    CFRelease(source);
    CFRelease(cfData);
    return result;
}

} // namespace

const char* getImageDecoderName()
{
    return "ImageIO";
}

#elif defined(BKLV_USE_STB_IMAGE)

// ===================== Linux / Windows / Switch: stb_image =====================
//
// 只带 PNG/JPEG/BMP：GIF 统一走 image_decoder_gif.cpp（那边单独实例化一份 stb，
// 带 STB_IMAGE_STATIC，所以两份实现不会符号冲突）。

// 第三方头文件的警告只在这一个编译单元里压掉（STB_IMAGE_STATIC 会让未用到的函数
// 全部变成 static，必然报 -Wunused-function）
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
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_NO_STDIO_WRITE

#include <stb_image.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace
{

// stb 返回 RGB/RGBA/灰度，统一转成非预乘 RGBA8（stb 给的就是直通 alpha）
DecodedImage toRgbaStb(unsigned char* data, int width, int height, int channels)
{
    DecodedImage result;
    if (data == nullptr || width <= 0 || height <= 0)
    {
        result.error = stbi_failure_reason() != nullptr ? stbi_failure_reason() : "stb_image 解码失败";
        if (data != nullptr)
            stbi_image_free(data);
        return result;
    }

    if (!isImageSizeAllowed(width, height, &result.error))
    {
        stbi_image_free(data);
        return result;
    }

    const size_t pixelCount = (size_t)width * (size_t)height;
    result.pixels.resize(pixelCount * 4);
    result.width  = width;
    result.height = height;
    result.stride = width * 4;

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

DecodedImage decodePlatformImage(const unsigned char* data, size_t size, const char* /*path*/)
{
    DecodedImage result;

    // 先按头部卡尺寸：畸形文件不能靠"解完再说"（会先把内存吃光）
    int declaredWidth  = 0;
    int declaredHeight = 0;
    if (probeImageSize(data, size, declaredWidth, declaredHeight)
        && !isImageSizeAllowed(declaredWidth, declaredHeight, &result.error))
        return result;

    int width = 0, height = 0, channels = 0;
    unsigned char* pixels =
        stbi_load_from_memory(data, (int)size, &width, &height, &channels, 0);
    return toRgbaStb(pixels, width, height, channels);
}

} // namespace

const char* getImageDecoderName()
{
    return "stb_image";
}

#else

// ============================ 无解码器 ============================

namespace
{

DecodedImage decodePlatformImage(const unsigned char*, size_t, const char*)
{
    DecodedImage result;
    result.error = "当前构建没有图片解码器";
    return result;
}

} // namespace

const char* getImageDecoderName()
{
    return "none";
}

#endif

// ---------------------------------------------------------------------------
// 统一入口（所有平台共用）
// ---------------------------------------------------------------------------

namespace
{

// 各个解码分支都会用；某些平台配置（Switch 走 stb + 内建的 GIF）下没人调它，
// 所以显式标一下，免得在别的工具链上变成 "defined but not used" 警告
[[maybe_unused]] bool readWholeFile(
    const char* path, std::vector<unsigned char>& out, std::string& error)
{
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr)
    {
        error = std::string("读不到文件：") + (path != nullptr ? path : "(null)");
        return false;
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (size <= 0)
    {
        std::fclose(file);
        error = std::string("文件是空的：") + path;
        return false;
    }

    out.resize((size_t)size);
    const size_t read = std::fread(out.data(), 1, out.size(), file);
    std::fclose(file);

    if (read != out.size())
    {
        out.clear();
        error = std::string("读取文件失败：") + path;
        return false;
    }

    return true;
}

// 平台解码器支持哪些格式（macOS 的 ImageIO 还能解 HEIC/TIFF/WebP 等，所以不拦）
bool isPlatformFormats(ImageFormat format)
{
    if (format == ImageFormat::PNG || format == ImageFormat::JPEG || format == ImageFormat::BMP)
        return true;

#if defined(__APPLE__) && !defined(BKLV_FORCE_STB_IMAGE)
    // ImageIO 覆盖面很广：认不出来的格式也让平台解码器试一把
    return true;
#else
    return false;
#endif
}

} // namespace

DecodedImage decodeImageMemory(const void* data, size_t size)
{
    DecodedImage result;
    if (data == nullptr || size == 0)
    {
        result.error = "图片数据为空";
        return result;
    }

    const ImageFormat format = detectImageFormat(data, size);

    if (format == ImageFormat::GIF)
    {
        // GIF 全平台同一条路：拿第一帧当静态图（动画用 decodeAnimationMemory）
        DecodedAnimation animation = detail::decodeGifAnimation(
            static_cast<const unsigned char*>(data), size);
        if (!animation.isValid())
        {
            result.error = animation.error.empty() ? "GIF 解码失败" : animation.error;
            return result;
        }

        result.pixels = std::move(animation.frames.front());
        result.width  = animation.width;
        result.height = animation.height;
        result.stride = animation.width * 4;
        return result;
    }

    if (format == ImageFormat::SVG)
    {
        // SVG 走 lunasvg 栅格化（尺寸用文档自带的）
        result = detail::decodeSvgMemory(static_cast<const unsigned char*>(data), size, 0, 0);
        return result;
    }

    if (format == ImageFormat::WEBP)
    {
#if defined(BKLV_USE_WEBP)
        // 有 libwebp 就统一走它（全平台同一份实现，避免桌面能解、Switch 不能解）
        result = detail::decodeWebPImage(static_cast<const unsigned char*>(data), size);
        return result;
#else
        // 没编 libwebp：交给平台解码器试（macOS 的 ImageIO 能解 WebP），
        // 其余平台会走到下面的"不支持"分支并说明原因
#endif
    }

    if (!isPlatformFormats(format))
    {
        if (format == ImageFormat::UNKNOWN)
            result.error = "无法识别的图片格式";
        else
            result.error = std::string("当前解码器（") + getImageDecoderName() + "）不支持 "
                + getImageFormatName(format) + " 图片";
        return result;
    }

    // 已知格式先卡尺寸；认不出的格式交给平台解码器自己判
    int declaredWidth  = 0;
    int declaredHeight = 0;
    if (probeImageSize(data, size, declaredWidth, declaredHeight)
        && !isImageSizeAllowed(declaredWidth, declaredHeight, &result.error))
        return result;

    result = decodePlatformImage(static_cast<const unsigned char*>(data), size, nullptr);
    if (!result.isValid())
    {
        // 连格式都认不出来时，把平台解码器的话也带上（macOS 的 ImageIO 还能兜 TGA 之类）
        if (format == ImageFormat::UNKNOWN)
        {
            const std::string detail = result.error.empty() ? "解码失败" : result.error;
            result.error = std::string("无法识别的图片格式（") + getImageDecoderName() + "："
                + detail + "）";
        }
        else if (result.error.empty())
            result.error = std::string(getImageDecoderName()) + " 解码失败";
    }
    return result;
}

DecodedImage decodeImageFile(const char* path)
{
    if (path == nullptr)
    {
        DecodedImage result;
        result.error = "路径为空";
        return result;
    }

    std::vector<unsigned char> bytes;
    std::string error;
    if (!readWholeFile(path, bytes, error))
    {
        DecodedImage result;
        result.error = error;
        return result;
    }

    return decodeImageMemory(bytes.data(), bytes.size());
}

DecodedAnimation decodeAnimationMemory(const void* data, size_t size)
{
    DecodedAnimation result;
    if (data == nullptr || size == 0)
    {
        result.error = "动画数据为空";
        return result;
    }

    const ImageFormat format = detectImageFormat(data, size);

    if (format == ImageFormat::WEBP)
    {
        // 只有编了 libwebp 的构建支持 WebP 动图（demux）
        return detail::decodeWebPAnimation(static_cast<const unsigned char*>(data), size);
    }

    if (format != ImageFormat::GIF)
    {
        result.error = std::string("暂不支持 ") + getImageFormatName(format)
            + " 动画（目前只有 GIF 与 WebP）";
        return result;
    }

    // 先按头部卡画布尺寸
    int declaredWidth  = 0;
    int declaredHeight = 0;
    if (probeImageSize(data, size, declaredWidth, declaredHeight)
        && !isImageSizeAllowed(declaredWidth, declaredHeight, &result.error))
        return result;

    return detail::decodeGifAnimation(static_cast<const unsigned char*>(data), size);
}

DecodedImage decodeSvgMemory(const void* data, size_t size, int width, int height)
{
    return detail::decodeSvgMemory(static_cast<const unsigned char*>(data), size, width, height);
}

DecodedImage decodeSvgFile(const char* path, int width, int height)
{
    if (path == nullptr)
    {
        DecodedImage result;
        result.error = "路径为空";
        return result;
    }

    std::vector<unsigned char> bytes;
    std::string error;
    if (!readWholeFile(path, bytes, error))
    {
        DecodedImage result;
        result.error = error;
        return result;
    }

    return detail::decodeSvgMemory(bytes.data(), bytes.size(), width, height);
}

DecodedAnimation decodeAnimationFile(const char* path)
{
    if (path == nullptr)
    {
        DecodedAnimation result;
        result.error = "路径为空";
        return result;
    }

    std::vector<unsigned char> bytes;
    std::string error;
    if (!readWholeFile(path, bytes, error))
    {
        DecodedAnimation result;
        result.error = error;
        return result;
    }

    return decodeAnimationMemory(bytes.data(), bytes.size());
}

} // namespace bklv
