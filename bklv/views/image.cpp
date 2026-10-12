#include "bklv/views/image.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/core/image_decoder.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/task.hpp"

namespace bklv
{

Image::Image()
{
    // 图片默认按内容尺寸撑开：没有显式尺寸时用图片自身大小。
    YGNodeStyleSetWidthAuto(this->ygNode);
    YGNodeStyleSetHeightAuto(this->ygNode);
}

Image::~Image() = default;

void Image::setImageFromFileAsync(const std::string& path)
{
    // 记下路径：纹理仓库没就绪时 update() 还会走同步重试那条路
    this->pendingPath = path;
    this->asyncPending = true;

    const uint64_t generation = ++this->loadGeneration;
    const std::string resolved = resourcePath(path);

    // 后台：读文件 + 解码（纯 CPU/IO，不碰 GL/ImGui）
    // 回调捕获的是 weak 护栏，不是裸 this：Image 先死了就直接放弃
    std::weak_ptr<const AliveFlag::Flag> guard = this->alive.handle();

    task::runResult(
        [resolved] { return decodeImageFile(resolved.c_str()); },
        [this, generation, guard](DecodedImage image) {
            if (guard.expired() || !guard.lock()->isAlive())
                return; // 视图已经销毁，别再碰它

            if (generation != this->loadGeneration)
                return; // 已经发起了更新的加载，这个结果丢掉

            this->asyncPending = false;

            if (!image.isValid() || !isTextureStoreReady())
            {
                // 仓库还没好：保留 pendingPath，交给 update() 的重试逻辑
                return;
            }

            const TextureRef loaded =
                loadTextureFromPixels(image.pixels.data(), image.width, image.height);
            if (!loaded.isValid())
                return;

            this->texture = loaded;
            this->pendingPath.clear();

            if (this->getWidth() <= 0.0f)
                this->setWidth(static_cast<float>(loaded.width));
            if (this->getHeight() <= 0.0f)
                this->setHeight(static_cast<float>(loaded.height));

            this->invalidate();
        });
}

bool Image::setImageFromFile(const std::string& path)
{
    // 记下路径：仓库没就绪时这一句会失败，但要留着让 update() 重试。
    this->pendingPath = path;

    const TextureRef loaded = loadTexture(resourcePath(path));
    if (!loaded.isValid())
        return false;

    this->texture = loaded;
    this->pendingPath.clear();

    // 没有显式尺寸时，用图片原始尺寸，避免退化成 0x0。
    if (this->getWidth() <= 0.0f)
        this->setWidth(static_cast<float>(loaded.width));
    if (this->getHeight() <= 0.0f)
        this->setHeight(static_cast<float>(loaded.height));

    return true;
}

void Image::update()
{
    // 视图树通常在 App::run() 之前就建好了，那时图形后端还没创建、纹理传不上去。
    // 这里在仓库就绪后补加载一次；成功后不再重试。
    if (this->texture.isValid() || this->pendingPath.empty() || !isTextureStoreReady())
        return;

    const std::string loading = this->pendingPath;
    const TextureRef loaded   = loadTexture(resourcePath(loading));
    if (!loaded.isValid())
        return;

    this->texture = loaded;
    this->pendingPath.clear();

    if (this->getWidth() <= 0.0f)
        this->setWidth(static_cast<float>(loaded.width));
    if (this->getHeight() <= 0.0f)
        this->setHeight(static_cast<float>(loaded.height));

    this->invalidate();
}

void Image::setImage(const TextureRef& ref)
{
    this->texture = ref;
    this->invalidate();
}

void Image::draw(ImDrawList* drawList, float x, float y, float width, float height, FrameContext*)
{
    if (width <= 0.0f || height <= 0.0f)
        return;

    const ImVec2 min(x, y);
    const ImVec2 max(x + width, y + height);

    // 没图就画占位色块，方便一眼看出"这里应该有图"。
    if (!this->texture.isValid())
    {
        if (this->placeholderColor != 0)
            drawList->AddRectFilled(min, max, colorWithAlpha(this->placeholderColor, this->getAlpha()),
                this->imageCornerRadius);
        return;
    }

    const float imageWidth  = static_cast<float>(this->texture.width);
    const float imageHeight = static_cast<float>(this->texture.height);

    // 按缩放方式算出目标矩形与 uv。
    ImVec2 destMin = min;
    ImVec2 destMax = max;
    ImVec2 uvMin(0.0f, 0.0f);
    ImVec2 uvMax(1.0f, 1.0f);

    switch (this->scalingType)
    {
        case ImageScalingType::STRETCH:
            // 直接用整个矩形
            break;

        case ImageScalingType::FIT:
        {
            // 保持比例，取较小的缩放比，居中留边
            const float scale = std::min(width / imageWidth, height / imageHeight);
            const float drawW = imageWidth * scale;
            const float drawH = imageHeight * scale;
            destMin           = ImVec2(x + (width - drawW) * 0.5f, y + (height - drawH) * 0.5f);
            destMax           = ImVec2(destMin.x + drawW, destMin.y + drawH);
            break;
        }

        case ImageScalingType::FILL:
        {
            // 保持比例，铺满矩形，超出部分通过 uv 裁掉
            const float scale = std::max(width / imageWidth, height / imageHeight);
            const float shownW = width / (imageWidth * scale);
            const float shownH = height / (imageHeight * scale);
            uvMin = ImVec2((1.0f - shownW) * 0.5f, (1.0f - shownH) * 0.5f);
            uvMax = ImVec2(1.0f - uvMin.x, 1.0f - uvMin.y);
            break;
        }

        case ImageScalingType::CENTER:
        {
            // 原尺寸居中
            const float drawW = std::min(imageWidth, width);
            const float drawH = std::min(imageHeight, height);
            destMin           = ImVec2(x + (width - drawW) * 0.5f, y + (height - drawH) * 0.5f);
            destMax           = ImVec2(destMin.x + drawW, destMin.y + drawH);
            break;
        }
    }

    const ImU32 tint = colorWithAlpha(IM_COL32_WHITE, this->getAlpha());

    if (this->imageCornerRadius > 0.0f)
        drawList->AddImageRounded(this->texture.texture, destMin, destMax, uvMin, uvMax, tint,
            this->imageCornerRadius);
    else
        drawList->AddImage(this->texture.texture, destMin, destMax, uvMin, uvMax, tint);
}


namespace
{

void setImageSrcAttr(View* v, const ResolvedValue& r)
{
    static_cast<Image*>(v)->setImageFromFile(r.text);
}

void setImageRadiusAttr(View* v, const ResolvedValue& r)
{
    static_cast<Image*>(v)->setCornerRadius(r.number);
}

void setImagePlaceholderAttr(View* v, const ResolvedValue& r)
{
    static_cast<Image*>(v)->setPlaceholderColor(r.color);
}

void setScalingTypeAttr(View* v, const ResolvedValue& r)
{
    Image* image = static_cast<Image*>(v);
    if (enumMatches(r.text, "fit"))
        image->setScalingType(ImageScalingType::FIT);
    else if (enumMatches(r.text, "fill"))
        image->setScalingType(ImageScalingType::FILL);
    else if (enumMatches(r.text, "stretch"))
        image->setScalingType(ImageScalingType::STRETCH);
    else if (enumMatches(r.text, "center"))
        image->setScalingType(ImageScalingType::CENTER);
    else
        warnUnknownEnumValue("scalingType", r.text, "fit|fill|stretch|center");
}

} // namespace

const AttributeTable& Image::attributes() const
{
    static const AttributeTable table = {
        { "src", AttributeKind::RESOURCE_PATH, setImageSrcAttr, nullptr },
        // Image 上的 cornerRadius 指**图片本身的圆角**（子类表优先于公共表里
        // 那个背景圆角），这才是 Image 上看得见的东西。
        { "cornerRadius", AttributeKind::NUMBER, setImageRadiusAttr, nullptr },
        { "scalingType", AttributeKind::ENUM, setScalingTypeAttr, "fit|fill|stretch|center" },
        { "placeholderColor", AttributeKind::COLOR, setImagePlaceholderAttr, nullptr },
    };
    return table;
}

} // namespace bklv
