// 图片解码：格式识别、上限、错误信息、alpha 契约、GIF 动画。
//
// 都在无窗口下跑（解码本身不碰 GL/ImGui）。fixture 在 resources/image_test/，
// 都是几十字节的小图：2x2 的 PNG/BMP/JPEG/GIF + 一个只声明了超大尺寸的 PNG +
// 一个截断的 PNG + 一个文本文件。GIF 那个是两帧（延迟 10cs / 20cs）的动图。

#include <cstdio>
#include <string>

#include "bklv/core/image_decoder.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/logger.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

std::string fixture(const char* name)
{
    return resourcePath(std::string("resources/image_test/") + name);
}

// 取第 (x,y) 个像素的 RGBA
void pixelAt(const DecodedImage& image, int x, int y, unsigned char out[4])
{
    const size_t offset = (size_t)y * (size_t)image.stride + (size_t)x * 4;
    for (int i = 0; i < 4; i++)
        out[i] = image.pixels[offset + (size_t)i];
}

} // namespace

int main()
{
    bklv::initResources();

    std::printf("== 1. 按内容识别格式（不看扩展名）==\n");
    {
        struct Case
        {
            const char* file;
            ImageFormat expected;
        };
        const Case cases[] = {
            { "solid.png", ImageFormat::PNG },
            { "tiny.jpg", ImageFormat::JPEG },
            { "tiny.bmp", ImageFormat::BMP },
            { "anim.gif", ImageFormat::GIF },
            { "not_an_image.txt", ImageFormat::UNKNOWN },
        };

        for (const Case& item : cases)
        {
            DecodedImage image = decodeImageFile(fixture(item.file).c_str());
            // 用文件内容再认一次（拿不到内容时直接看文件头也行，这里走内存版本）
            std::string error;
            const std::string path = fixture(item.file);
            // 直接读文件内容交给 detectImageFormat
            std::FILE* file = std::fopen(path.c_str(), "rb");
            unsigned char head[16] = { 0 };
            size_t read            = file != nullptr ? std::fread(head, 1, sizeof(head), file) : 0;
            if (file != nullptr)
                std::fclose(file);

            checkEqualInt((int)detectImageFormat(head, read), (int)item.expected,
                std::string("格式识别正确：") + item.file);
        }

        // 其余格式只断言"认得出"（本机不一定有对应 fixture；非 macOS 上它们会被明确拒绝）
        const unsigned char tiffLe[] = { 'I', 'I', '*', 0 };
        const unsigned char heic[]   = { 0, 0, 0, 0x18, 'f', 't', 'y', 'p', 'h', 'e', 'i', 'c' };
        const unsigned char avif[]   = { 0, 0, 0, 0x18, 'f', 't', 'y', 'p', 'a', 'v', 'i', 'f' };
        const unsigned char webp[]   = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P' };
        const unsigned char ico[]    = { 0, 0, 1, 0, 1, 0 };
        checkEqualInt((int)detectImageFormat(tiffLe, sizeof(tiffLe)), (int)ImageFormat::TIFF,
            "认出 TIFF");
        checkEqualInt((int)detectImageFormat(heic, sizeof(heic)), (int)ImageFormat::HEIC,
            "认出 HEIC");
        checkEqualInt((int)detectImageFormat(avif, sizeof(avif)), (int)ImageFormat::HEIC,
            "认出 AVIF（同属 ftyp 一族）");
        checkEqualInt((int)detectImageFormat(webp, sizeof(webp)), (int)ImageFormat::WEBP,
            "认出 WebP");
        checkEqualInt((int)detectImageFormat(ico, sizeof(ico)), (int)ImageFormat::ICO, "认出 ICO");
    }

    std::printf("\n== 2. 各格式都能解码出 RGBA8（stride = width * 4）==\n");
    {
        const char* files[] = { "solid.png", "tiny.jpg", "tiny.bmp", "anim.gif" };
        for (const char* name : files)
        {
            DecodedImage image = decodeImageFile(fixture(name).c_str());
            check(image.isValid(), std::string("解码成功：") + name);
            checkEqualInt(image.width, 2, std::string("宽 2：") + name);
            checkEqualInt(image.height, 2, std::string("高 2：") + name);
            checkEqualInt(image.stride, image.width * 4, std::string("stride = 宽 × 4：") + name);
            checkEqualSize(image.pixels.size(), (size_t)(image.stride * image.height),
                std::string("像素缓冲长度正确：") + name);
            check(image.getError().empty(), std::string("成功时没有错误信息：") + name);
        }

        // 仓库里那张大图也过一遍
        DecodedImage testcard = decodeImageFile(resourcePath("resources/testcard.png").c_str());
        check(testcard.isValid(), "resources/testcard.png 能解码");
        check(testcard.width > 100 && testcard.height > 100, "testcard 尺寸正常");
    }

    std::printf("\n== 3. alpha 契约：解出来必须是**非预乘** RGBA ==\n");
    {
        // 这张 2x2 的四像素依次是：
        //   (255,0,0,255) 不透明红 / (255,0,0,128) 半透明红 / (0,0,0,0) 全透明 / (0,255,0,255) 不透明绿
        // macOS 的 CGBitmapContext 只能给预乘结果（半透明红会变成 (128,0,0,128)），
        // 所以解码器必须做反预乘 —— 这条断言就是钉这个契约的。
        DecodedImage image = decodeImageFile(fixture("alpha.png").c_str());
        check(image.isValid(), "带 alpha 的 PNG 能解码");

        unsigned char px[4] = { 0 };
        pixelAt(image, 0, 0, px);
        checkColor(IM_COL32(px[0], px[1], px[2], px[3]), IM_COL32(255, 0, 0, 255),
            "不透明红不变");

        pixelAt(image, 1, 0, px);
        checkEqualInt(px[3], 128, "半透明像素的 alpha 是 128");
        checkEqualInt(px[0], 255, "半透明红解出来是**非预乘**的 255（不是 128）");
        checkEqualInt(px[1], 0, "半透明红的绿分量是 0");

        pixelAt(image, 0, 1, px);
        checkEqualInt(px[3], 0, "全透明像素 alpha 为 0");

        pixelAt(image, 1, 1, px);
        checkColor(IM_COL32(px[0], px[1], px[2], px[3]), IM_COL32(0, 255, 0, 255),
            "不透明绿不变");
    }

    std::printf("\n== 4. 尺寸上限：先卡头，不解超大图 ==\n");
    {
        const std::string path = fixture("huge.png");
        const std::string text = path;

        int width = 0, height = 0;
        std::FILE* file = std::fopen(text.c_str(), "rb");
        check(file != nullptr, "能打开超大图 fixture");
        if (file != nullptr)
        {
            std::fseek(file, 0, SEEK_END);
            const long size = std::ftell(file);
            std::fseek(file, 0, SEEK_SET);
            std::vector<unsigned char> bytes((size_t)size);
            const size_t read = std::fread(bytes.data(), 1, bytes.size(), file);
            std::fclose(file);

            checkEqualSize(read, bytes.size(), "读到了 fixture 的全部字节");
            check(probeImageSize(bytes.data(), bytes.size(), width, height), "能从头部读出尺寸");
            checkEqualInt(width, 20000, "头部声明的宽是 20000");
            checkEqualInt(height, 20000, "头部声明的高是 20000");
        }

        DecodedImage image = decodeImageFile(path.c_str());
        check(!image.isValid(), "超限图片被拒绝");
        check(image.getError().find("超限") != std::string::npos,
            "错误信息说明是尺寸超限：" + image.getError());
    }

    std::printf("\n== 5. 坏数据 / 非图片：失败要带人话错误 ==\n");
    {
        DecodedImage broken = decodeImageFile(fixture("broken.png").c_str());
        check(!broken.isValid(), "截断的 PNG 解码失败");
        check(!broken.getError().empty(), "截断 PNG 有错误信息：" + broken.getError());

        DecodedImage text = decodeImageFile(fixture("not_an_image.txt").c_str());
        check(!text.isValid(), "文本文件解码失败");
        check(text.getError().find("无法识别") != std::string::npos,
            "文本文件的错误信息是「无法识别」：" + text.getError());

        DecodedImage missing = decodeImageFile(fixture("no_such_file.png").c_str());
        check(!missing.isValid(), "文件不存在时失败");
        check(missing.getError().find("读不到文件") != std::string::npos,
            "文件不存在时说明是读不到文件：" + missing.getError());

        DecodedImage empty = decodeImageMemory(nullptr, 0);
        check(!empty.isValid() && !empty.getError().empty(), "空数据失败并带原因");
    }

    std::printf("\n== 6. GIF 动画：帧数 / 延迟 / 帧内容 ==\n");
    {
        DecodedAnimation animation = decodeAnimationFile(fixture("anim.gif").c_str());
        check(animation.isValid(), "GIF 动画解码成功（失败原因：" + animation.getError() + "）");
        if (!animation.isValid())
        {
            std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
            return 1;
        }
        checkEqualSize(animation.getFrameCount(), 2, "两帧");
        checkEqualInt(animation.width, 2, "画布宽 2");
        checkEqualInt(animation.height, 2, "画布高 2");
        check(animation.loop, "按循环播放处理");

        checkFloat(animation.delays[0], 0.1f, "第一帧延迟 100ms");
        checkFloat(animation.delays[1], 0.2f, "第二帧延迟 200ms");
        checkFloat(animation.getDuration(), 0.3f, "一轮总时长 0.3s");

        for (const std::vector<unsigned char>& frame : animation.frames)
            checkEqualSize(frame.size(), (size_t)(animation.width * animation.height * 4),
                "每帧都是整幅画布");

        check(animation.frames[0] != animation.frames[1], "两帧内容不同");

        // 静态入口拿到的必须是第一帧
        DecodedImage first = decodeImageFile(fixture("anim.gif").c_str());
        check(first.isValid(), "GIF 也能当静态图解（取第一帧）");
        check(first.pixels == animation.frames[0], "静态入口给的正是第一帧");
    }

    std::printf("\n== 7. 没有动画解码器的格式要明确报错 ==\n");
    {
        DecodedAnimation pngAnimation = decodeAnimationFile(fixture("solid.png").c_str());
        check(!pngAnimation.isValid(), "PNG 没有动画");
        check(pngAnimation.getError().find("暂不支持") != std::string::npos,
            "PNG 的错误信息说明暂不支持：" + pngAnimation.getError());

        DecodedAnimation none = decodeAnimationMemory(nullptr, 0);
        check(!none.isValid() && !none.getError().empty(), "空动画数据失败并带原因");
    }

    std::printf("\n== 8. 当前解码器信息 ==\n");
    {
        check(getImageDecoderName() != nullptr && getImageDecoderName()[0] != 0, "解码器名非空");
        std::printf("  [数据] 平台解码器 = %s\n", getImageDecoderName());
        std::printf("  [数据] 上限：单边 %d，单帧 %zu 像素，动画 %zu 像素 / %zu 帧\n",
            getImageLimits().maxDimension, getImageLimits().maxPixels,
            getImageLimits().maxAnimationPixels, getImageLimits().maxAnimationFrames);
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
