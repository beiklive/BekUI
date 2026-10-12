#include "designer/core/image_diff.hpp"

#include <vector>

#include "bklv/core/image_decoder.hpp"
#include "bklv/core/image_writer.hpp"

namespace designer
{

namespace
{

// DecodedImage 统一是 RGBA8
unsigned char sample(const bklv::DecodedImage& image, int x, int y, int channel)
{
    const size_t index = ((size_t)y * (size_t)image.width + (size_t)x) * 4 + (size_t)channel;
    if (index >= image.pixels.size())
        return 0;
    return image.pixels[index];
}

} // namespace

ImageDiff compareImages(const std::string& pathA, const std::string& pathB, std::string& error)
{
    ImageDiff result;

    const bklv::DecodedImage a = bklv::decodeImageFile(pathA.c_str());
    if (!a.isValid())
    {
        error = "读不了图：" + pathA;
        return result;
    }
    const bklv::DecodedImage b = bklv::decodeImageFile(pathB.c_str());
    if (!b.isValid())
    {
        error = "读不了图：" + pathB;
        return result;
    }

    result.width      = a.width;
    result.height     = a.height;
    result.otherWidth = b.width;
    result.otherHeight = b.height;
    result.sameSize   = (a.width == b.width && a.height == b.height);
    if (!result.sameSize)
        return result;

    long long total   = 0;
    long long changed = 0;
    const long long pixelCount = (long long)a.width * (long long)a.height;
    for (int y = 0; y < a.height; y++)
    {
        for (int x = 0; x < a.width; x++)
        {
            int worst = 0;
            for (int channel = 0; channel < 3; channel++) // 只看 RGB，alpha 差异不算"看得见的变化"
            {
                const int diff = std::abs((int)sample(a, x, y, channel)
                    - (int)sample(b, x, y, channel));
                total += diff;
                if (diff > worst)
                    worst = diff;
            }
            if (worst > result.maxDiff)
                result.maxDiff = worst;
            if (worst > 8)
                changed++;
        }
    }

    if (pixelCount > 0)
    {
        result.meanDiff     = (float)((double)total / (double)(pixelCount * 3));
        result.changedRatio = (float)((double)changed / (double)pixelCount);
    }
    return result;
}

bool writeDiffImage(const std::string& outputPath, const std::string& pathA,
    const std::string& pathB, std::string& error)
{
    const bklv::DecodedImage a = bklv::decodeImageFile(pathA.c_str());
    const bklv::DecodedImage b = bklv::decodeImageFile(pathB.c_str());
    if (!a.isValid() || !b.isValid() || a.width != b.width || a.height != b.height)
    {
        error = "两张图尺寸不一致或读不了，无法生成差异图";
        return false;
    }

    std::vector<unsigned char> output((size_t)a.width * (size_t)a.height * 4);
    for (int y = 0; y < a.height; y++)
    {
        for (int x = 0; x < a.width; x++)
        {
            int worst = 0;
            for (int channel = 0; channel < 3; channel++)
                worst = std::max(worst,
                    std::abs((int)sample(a, x, y, channel) - (int)sample(b, x, y, channel)));

            const size_t index = ((size_t)y * (size_t)a.width + (size_t)x) * 4;
            if (worst <= 8)
            {
                const unsigned char grey
                    = (unsigned char)((sample(a, x, y, 0) + sample(a, x, y, 1)
                                          + sample(a, x, y, 2)) / 6);
                output[index + 0] = grey;
                output[index + 1] = grey;
                output[index + 2] = grey;
            }
            else
            {
                const bool brighter = sample(a, x, y, 1) > sample(b, x, y, 1);
                output[index + 0] = brighter ? 255 : 40;   // 变亮标红
                output[index + 1] = 40;
                output[index + 2] = brighter ? 40 : 255;   // 变暗标蓝
            }
            output[index + 3] = 255;
        }
    }

    return bklv::writePngFile(outputPath, output.data(), a.width, a.height, 4);
}

} // namespace designer
