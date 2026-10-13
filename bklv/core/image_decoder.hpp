#pragma once

// 图片解码抽象：把 PNG/JPEG/BMP/GIF（含 GIF 动画）解成 RGBA8 像素。
//
// 与图形 API 无关，只跟平台有关：
//   macOS            —— ImageIO（系统自带，零依赖；顺带支持 HEIC/TIFF/WebP 等）
//   Linux/Win/Switch —— stb_image（源码随仓库，跨平台）
//   GIF（所有平台）   —— 自带的 stb_image GIF 解码器（image_decoder_gif.cpp）
//
// 三条约定（新增解码器时必须遵守）：
//   ① 输出**非预乘（straight）RGBA8**：ImGui 的混合方程是
//      `SRC_ALPHA / ONE_MINUS_SRC_ALPHA`（GL 与 Vulkan 都是），预乘图整体会偏暗。
//      macOS 的 CGBitmapContext 只支持预乘 alpha，所以那边解码后要做一次反预乘。
//   ② 失败必须填 `DecodedImage::error`（一句人话），不要只返回空图。
//   ③ 解码前先按**内容**认格式 + 用 `probeImageSize()` 卡尺寸上限，
//      畸形文件不能靠"解完再说"来兜（会先把内存吃光）。

#include <cstddef>
#include <string>
#include <vector>

namespace bklv
{

// 按**文件内容**识别的格式（不看扩展名）
enum class ImageFormat
{
    UNKNOWN,
    PNG,
    JPEG,
    BMP,
    GIF,
    WEBP,   // 只有 macOS 的 ImageIO 能解
    TIFF,   // 同上
    HEIC,   // HEIF/HEIC/AVIF 一族（ftyp box）
    ICO,    // 同上
};

// 只读开头几个字节就能认出来；认不出返回 UNKNOWN
ImageFormat detectImageFormat(const void* data, size_t size);
const char* getImageFormatName(ImageFormat format);

// 尺寸/像素上限：超过就拒绝解码（畸形 PNG 能声明天文数字尺寸，Switch 上会直接 OOM）。
// 这是"解码前的第一道闸"，不是渲染限制 —— 真要放更大的图就改这几个值。
struct ImageLimits
{
    int maxDimension          = 8192;          // 单边最大像素
    size_t maxPixels          = 3840u * 2160u; // 单帧最大总像素（4K）
    size_t maxAnimationPixels = 16000000u;     // 动画所有帧加起来的像素（约 64MB RGBA8）
    size_t maxAnimationFrames = 512;           // 动画最大帧数
};
ImageLimits getImageLimits();

// 从图片头部读出宽高（PNG/JPEG/BMP/GIF）。读不出来返回 false
// （不一定是坏图 —— 交给解码器判，解码失败时会带上原因）。
bool probeImageSize(const void* data, size_t size, int& width, int& height);

// 尺寸是否在上限之内。超限时把原因写进 `error`（若非空）。
bool isImageSizeAllowed(int width, int height, std::string* error);

struct DecodedImage
{
    std::vector<unsigned char> pixels; // 非预乘 RGBA8，长度为 stride * height
    int width  = 0;
    int height = 0;
    int stride = 0; // 一行的字节数（目前恒等于 width * 4）
    std::string error;

    bool isValid() const
    {
        return !this->pixels.empty() && this->width > 0 && this->height > 0 && this->stride > 0;
    }
    // 失败原因；成功时是空串
    const std::string& getError() const { return this->error; }
};

// 一段动画。每帧都是**已经合成好的整幅画布**（帧内局部矩形、透明、disposal 都由
// 解码器处理完），所以播放侧只管按延迟换帧。
struct DecodedAnimation
{
    int width  = 0;
    int height = 0;
    std::vector<std::vector<unsigned char>> frames; // 每帧 stride * height（非预乘 RGBA8）
    std::vector<float> delays;                      // 每帧显示多久（秒），与 frames 等长
    bool loop = true;
    std::string error;

    size_t getFrameCount() const { return this->frames.size(); }
    bool isValid() const { return !this->frames.empty() && this->width > 0 && this->height > 0; }
    // 一轮总时长（秒）；空动画返回 0
    float getDuration() const;
    const std::string& getError() const { return this->error; }
};

// 按"已经播了多久"算该显示第几帧：内部对一轮总时长**取模**，所以不会累积漂移，
// 卡帧/切页回来也不会补播一堆帧。elapsed 为负按 0 处理；空动画返回 0。
size_t frameIndexAt(const DecodedAnimation& animation, float elapsedSeconds);

// 从文件解码（内部读文件 → 走 decodeImageMemory）。失败返回的 isValid() 为 false。
DecodedImage decodeImageFile(const char* path);

// 从内存里的图片数据解码。GIF 走自带的 GIF 解码器（取第一帧），其余交给平台解码器。
DecodedImage decodeImageMemory(const void* data, size_t size);

// 动画解码：目前只有 GIF（stb 的 GIF 解码器给出合成好的整幅帧 + 帧延迟，单位毫秒）。
// 其它格式返回 isValid() 为 false，error 说明原因。
DecodedAnimation decodeAnimationFile(const char* path);
DecodedAnimation decodeAnimationMemory(const void* data, size_t size);

// 当前使用的平台解码器名字（"ImageIO" / "stb_image" / "none"），调试层展示用。
const char* getImageDecoderName();

} // namespace bklv
