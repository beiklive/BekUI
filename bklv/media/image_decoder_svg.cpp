// SVG 栅格化解码。lunasvg + plutovg 是可选依赖（CMake 的 BKLV_WITH_SVG）。
//
// 和位图不同的地方：**SVG 没有固定的像素尺寸**，必须给一个栅格化尺寸。
// 所以除了走统一入口（用文档自带的 width/height/viewBox 尺寸），还提供
// `decodeSvgFile/Memory(path, data, size, width, height)` 显式指定尺寸 ——
// 视图按自己的布局尺寸调用它，尺寸变了就重新栅格化（见 views/image.cpp）。
//
// 像素格式：lunasvg 的 Bitmap 是 **BGRA + 预乘 alpha**（plutovg 的约定），
// 而本项目的契约是 **RGBA + 非预乘**（ImGui 的混合方程是直通 alpha）。
// 所以这里要做两步转换：通道顺序 + 反预乘。这是最容易被忽略的一步 ——
// 不做的话半透明 SVG 会整体偏暗（和 macOS 的 ImageIO 那条路踩过的是同一个坑）。

#include <cstring>
#include <string>

#include "bklv/media/image_decoder.hpp"

#if defined(BKLV_USE_SVG)

#include <lunasvg.h>

namespace bklv
{
namespace detail
{

namespace
{

DecodedImage rasterize(lunasvg::Document* document, int width, int height)
{
    DecodedImage result;

    if (document == nullptr)
    {
        result.error = "SVG 解析失败";
        return result;
    }
    if (!isImageSizeAllowed(width, height, &result.error))
        return result;

    lunasvg::Bitmap bitmap = document->renderToBitmap(width, height);
    if (bitmap.isNull() || bitmap.width() <= 0 || bitmap.height() <= 0 || bitmap.data() == nullptr)
    {
        result.error = "SVG 栅格化失败";
        return result;
    }

    // lunasvg 的位图是 ARGB32 **预乘**，而本项目契约是非预乘 RGBA ——
    // 它自带这个转换（convertToRGBA），别自己手写（手写容易漏掉通道顺序）。
    bitmap.convertToRGBA();

    const int outWidth  = bitmap.width();
    const int outHeight = bitmap.height();
    const int outStride = outWidth * 4;

    result.pixels.resize((size_t)outStride * (size_t)outHeight);
    for (int y = 0; y < outHeight; y++)
    {
        std::memcpy(result.pixels.data() + (size_t)y * (size_t)outStride,
            bitmap.data() + (size_t)y * (size_t)bitmap.stride(), (size_t)outStride);
    }

    result.width  = outWidth;
    result.height = outHeight;
    result.stride = outStride;
    return result;
}

} // namespace

bool isSvgSupported()
{
    return true;
}

DecodedImage decodeSvgMemory(const unsigned char* data, size_t size, int width, int height)
{
    DecodedImage result;
    if (data == nullptr || size == 0)
    {
        result.error = "SVG 数据为空";
        return result;
    }

    std::unique_ptr<lunasvg::Document> document =
        lunasvg::Document::loadFromData(reinterpret_cast<const char*>(data), size);
    if (document == nullptr)
    {
        result.error = "SVG 解析失败（XML 不合法或不支持）";
        return result;
    }

    // 没给尺寸就用文档自带的（width/height 或 viewBox）
    int targetWidth  = width;
    int targetHeight = height;
    if (targetWidth <= 0 || targetHeight <= 0)
    {
        targetWidth  = (int)(document->width() + 0.5f);
        targetHeight = (int)(document->height() + 0.5f);
    }

    if (targetWidth <= 0 || targetHeight <= 0)
    {
        result.error = "SVG 没有自带尺寸，栅格化时必须显式指定宽高";
        return result;
    }

    return rasterize(document.get(), targetWidth, targetHeight);
}

} // namespace detail
} // namespace bklv

#else // !BKLV_USE_SVG

namespace bklv
{
namespace detail
{

bool isSvgSupported()
{
    return false;
}

DecodedImage decodeSvgMemory(const unsigned char*, size_t, int, int)
{
    DecodedImage result;
    result.error = "这个构建没有编 SVG 支持（CMake 的 BKLV_WITH_SVG=OFF，或 lunasvg/plutovg 子模块缺失）";
    return result;
}

} // namespace detail
} // namespace bklv

#endif
