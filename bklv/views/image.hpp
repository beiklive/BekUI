#pragma once

// 图片视图，对应 borealis 的 views/image.hpp。
//
// 与原版差异：NanoVG 的 nvgImagePattern 换成 ImDrawList 的 AddImage / AddImageRounded，
// 纹理来自 core/graphics.hpp 的纹理管理，由当前 RenderBackend 负责真正上传。
// 这里不出现任何图形 API 调用，所以换成 Vulkan/Metal 后端时本文件不用改。

#include "bklv/core/alive.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/core/image_decoder.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

// 图片在既定矩形里的缩放方式
enum class ImageScalingType
{
    FIT,    // 保持比例，完整放进矩形（留边）
    FILL,   // 保持比例，铺满矩形（超出部分裁掉）
    STRETCH, // 拉伸填满，不保持比例
    CENTER, // 原尺寸居中，不缩放
};

class Image : public View
{
  public:
    Image();
    ~Image() override;

    // 从文件加载。失败时保持原图不变。
    // path 是**相对资源目录**的路径（如 "resources/testcard.png"），
    // 内部会按平台解析成 romfs:/... 或 <可执行文件目录>/...
    // 见 core/resource.hpp；绝对路径原样使用。
    bool setImageFromFile(const std::string& path);

    // 异步加载：文件 IO 与解码在后台线程，纹理上传回到主线程。
    // 大图/网络路径用它就不会卡帧；期间保留旧纹理（不闪），
    // 重复调用以最后一次为准（旧结果按代数丢弃）。
    void setImageFromFileAsync(const std::string& path);
    bool isLoading() const { return !this->pendingPath.empty() || this->asyncPending; }
    // 直接用一个外部纹理（比如自己上传的）。
    void setImage(const TextureRef& ref);

    // ---- 动画（目前只有 GIF）----
    //
    // 播放用的就是**项目自带的帧时钟**（core/time.hpp 的 getFrameDeltaSeconds()）：
    // update() 里累加时间 → frameIndexAt() 取模算帧号 → 帧变了才 updateTexture() 就地换内容
    // （不重建纹理，也不参与布局）。不可见时不推进，切回来按绝对时间对齐，不会补播。
    bool setAnimationFromFile(const std::string& path);
    bool setAnimationFromMemory(const void* data, size_t size);

    bool hasAnimation() const { return !this->animation.frames.empty(); }
    size_t getAnimationFrameCount() const { return this->animation.frames.size(); }
    size_t getAnimationFrameIndex() const { return this->animationFrame; }
    float getAnimationTime() const { return this->animationElapsed; }
    void setAnimationPlaying(bool playing) { this->animationPlaying = playing; }
    bool isAnimationPlaying() const { return this->animationPlaying; }

    // 确定性步进（测试用）：不依赖帧时钟，直接把时间往前推
    void stepAnimation(float deltaSeconds);

    const TextureRef& getTexture() const { return this->texture; }
    bool hasImage() const { return this->texture.isValid(); }

    // 视图树可能先于图形后端就绪而建好，那时图片上传不了。
    // update() 会在仓库就绪后把没加载上的图补一次。
    void update() override;

    void setScalingType(ImageScalingType type) { this->scalingType = type; }
    ImageScalingType getScalingType() const { return this->scalingType; }

    // 圆角。等于 min(width, height)/2 时就是圆形。
    void setCornerRadius(float radius) { this->imageCornerRadius = radius; }
    float getImageCornerRadius() const { return this->imageCornerRadius; }

    // 加载失败或未设置图片时的占位色。
    void setPlaceholderColor(ImU32 color) { this->placeholderColor = color; }

    std::string getClassString() const override { return "bklv::Image"; }

    const bklv::AttributeTable& attributes() const override;

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, FrameContext* ctx) override;

    TextureRef texture;
    bool asyncPending = false;   // 正在后台解码
    uint64_t loadGeneration = 0; // 代数：只接受最新一次的异步结果
    // 异步解码回调的生命周期护栏：Image 先被销毁时回调直接放弃（否则是 use-after-free）
    AliveFlag alive;
    std::string pendingPath; // 还没加载成功的资源路径，等着 update() 重试
    // 动图：解好的帧 + 播放状态
    DecodedAnimation animation;
    float animationElapsed      = 0.0f;
    size_t animationFrame       = 0;
    bool animationPlaying       = true;
    bool animationPendingUpload = false; // 后端还没就绪时留给 update() 补上传
    // 纹理是不是本视图自己上传的动图纹理（是的话析构/换图时要释放，别漏）
    bool ownsTexture = false;

    // 释放自己上传的纹理（不动缓存里的静态图）
    void releaseOwnedTexture();
    bool applyAnimation(DecodedAnimation loaded);
    void uploadAnimationFrame();

    ImageScalingType scalingType = ImageScalingType::FIT;
    float imageCornerRadius      = 0.0f;
    ImU32 placeholderColor       = IM_COL32(45, 49, 56, 255);
};

} // namespace bklv
