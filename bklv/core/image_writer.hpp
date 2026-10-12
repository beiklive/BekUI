#pragma once

// PNG 写出（截图用）。解码在 core/image_decoder.*，这里只管编码。

#include <string>

namespace bklv
{

// 把 RGBA8 像素写成 PNG。channels 支持 4(RGBA) / 3(RGB)。
// 成功返回 true。任何平台都可用（stb_image_write，单头文件）。
bool writePngFile(const std::string& path, const unsigned char* pixels, int width, int height,
    int channels = 4);

} // namespace bklv
