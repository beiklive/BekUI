#pragma once

// 图片解码抽象：把 PNG/JPEG 之类的文件解成 RGBA8 像素。
//
// 与图形 API 无关，只跟平台有关：
//   macOS         —— ImageIO（系统自带，零依赖）
//   Linux / Switch —— stb_image（源码随仓库，跨平台）
//
// 这样 core/graphics.cpp 不再直接依赖任何平台的解码 API。

#include <cstddef>
#include <vector>

namespace bklv
{

struct DecodedImage
{
    std::vector<unsigned char> pixels; // RGBA8，长度为 width * height * 4
    int width  = 0;
    int height = 0;

    bool isValid() const { return !this->pixels.empty() && this->width > 0 && this->height > 0; }
};

// 从文件解码。失败返回的 DecodedImage::isValid() 为 false。
DecodedImage decodeImageFile(const char* path);

// 从内存里的 PNG/JPEG 数据解码。
DecodedImage decodeImageMemory(const void* data, size_t size);

// 当前使用的解码器名字，调试层展示用。
const char* getImageDecoderName();

} // namespace bklv
