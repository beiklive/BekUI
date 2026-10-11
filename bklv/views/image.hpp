#pragma once

// 图片视图，对应 borealis 的 views/image.hpp。
//
// 与原版差异：NanoVG 的 nvgImagePattern 换成 ImDrawList 的 AddImage / AddImageRounded，
// 纹理来自 core/graphics.hpp 的纹理管理，由当前 RenderBackend 负责真正上传。
// 这里不出现任何图形 API 调用，所以换成 Vulkan/Metal 后端时本文件不用改。

#include "bklv/core/graphics.hpp"
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
    bool setImageFromFile(const std::string& path);
    // 直接用一个外部纹理（比如自己上传的）。
    void setImage(const TextureRef& ref);

    const TextureRef& getTexture() const { return this->texture; }
    bool hasImage() const { return this->texture.isValid(); }

    void setScalingType(ImageScalingType type) { this->scalingType = type; }
    ImageScalingType getScalingType() const { return this->scalingType; }

    // 圆角。等于 min(width, height)/2 时就是圆形。
    void setCornerRadius(float radius) { this->imageCornerRadius = radius; }
    float getImageCornerRadius() const { return this->imageCornerRadius; }

    // 加载失败或未设置图片时的占位色。
    void setPlaceholderColor(ImU32 color) { this->placeholderColor = color; }

    std::string getClassString() const override { return "bklv::Image"; }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, Style style,
        FrameContext* ctx) override;

    TextureRef texture;
    ImageScalingType scalingType = ImageScalingType::FIT;
    float imageCornerRadius      = 0.0f;
    ImU32 placeholderColor       = IM_COL32(45, 49, 56, 255);
};

} // namespace bklv
