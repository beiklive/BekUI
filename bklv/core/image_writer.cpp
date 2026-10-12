#include "bklv/core/image_writer.hpp"

#include "bklv/core/logger.hpp"

// 第三方单头文件的警告：只在这一个编译单元里压掉，不动全局警告开关
// （项目的 -Wall -Wextra -Wpedantic 零警告约定只针对我们自己的代码）
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace bklv
{

bool writePngFile(const std::string& path, const unsigned char* pixels, int width, int height,
    int channels)
{
    if (path.empty() || pixels == nullptr || width <= 0 || height <= 0)
        return false;

    const int stride = width * channels;
    return stbi_write_png(path.c_str(), width, height, channels, pixels, stride) != 0;
}

} // namespace bklv
