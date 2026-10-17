#include "bklv/views/image.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "bklv/core/frame_budget.hpp"
#include "bklv/core/image_decoder.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/core/decode_queue.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/task.hpp"
#include "bklv/core/time.hpp"

namespace bklv
{

Image::Image()
{
    // 图片默认按内容尺寸撑开：没有显式尺寸时用图片自身大小。
    YGNodeStyleSetWidthAuto(this->ygNode);
    YGNodeStyleSetHeightAuto(this->ygNode);
}

Image::~Image()
{
    // 动图纹理会占用一张真纹理，必须自己释放（静态图是路径缓存持有的，不能动）
    this->releaseOwnedTexture();
    // 静态图：只放掉引用计数（纹理本身仍由路径缓存持有），供 2d 的淘汰判断
    if (!this->ownsTexture)
        releaseTexture(this->texture.index);
}

void Image::releaseOwnedTexture()
{
    if (!this->ownsTexture)
        return;

    if (this->texture.isValid())
        releaseTexture(this->texture);

    this->texture = TextureRef();
    this->ownsTexture = false;
}

bool Image::setAnimationFromFile(const std::string& path)
{
    DecodedAnimation loaded = decodeAnimationFile(resourcePath(path).c_str());
    if (!loaded.isValid())
    {
        Logger::warning("动图加载失败：" + path
            + (loaded.getError().empty() ? std::string() : "（" + loaded.getError() + "）"));
        return false;
    }

    return this->applyAnimation(std::move(loaded));
}

bool Image::setAnimationFromMemory(const void* data, size_t size)
{
    DecodedAnimation loaded = decodeAnimationMemory(data, size);
    if (!loaded.isValid())
    {
        Logger::warning("动图解码失败："
            + (loaded.getError().empty() ? std::string("未知原因") : loaded.getError()));
        return false;
    }

    return this->applyAnimation(std::move(loaded));
}

bool Image::setSvgFromFile(const std::string& path)
{
    std::FILE* file = std::fopen(resourcePath(path).c_str(), "rb");
    if (file == nullptr)
    {
        Logger::warning("SVG 读不到文件：" + path);
        return false;
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    std::vector<unsigned char> bytes;
    if (size > 0)
    {
        bytes.resize((size_t)size);
        if (std::fread(bytes.data(), 1, bytes.size(), file) != bytes.size())
            bytes.clear();
    }
    std::fclose(file);

    if (bytes.empty())
    {
        Logger::warning("SVG 读不到内容：" + path);
        return false;
    }

    return this->setSvgFromMemory(bytes.data(), bytes.size());
}

bool Image::setSvgFromMemory(const void* data, size_t size)
{
    if (data == nullptr || size == 0)
        return false;

    this->releaseOwnedTexture();

    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    this->svgData.assign(bytes, bytes + size);
    this->svgRasterWidth  = 0;
    this->svgRasterHeight = 0;
    this->animation       = DecodedAnimation(); // SVG 与动图互斥
    this->pendingPath.clear();

    // 立刻按当前尺寸栅格化一次（尺寸为 0 时用文档自带尺寸）
    this->rasterizeSvg();
    return !this->svgData.empty();
}

void Image::rasterizeSvg()
{
    if (this->svgData.empty() || !isTextureStoreReady())
        return;

    int width  = (int)(this->getWidth() + 0.5f);
    int height = (int)(this->getHeight() + 0.5f);

    // 布局还没算出尺寸时（auto 尺寸）先按文档自带的来
    if (width <= 0 || height <= 0)
    {
        width  = 0;
        height = 0;
    }

    if (width == this->svgRasterWidth && height == this->svgRasterHeight
        && this->texture.isValid())
        return; // 尺寸没变，不用重来

    // 帧预算用完就推迟：栅格化（lunasvg）+ 上传都在主线程，改一次窗口尺寸会一次撞上所有 SVG。
    // 尺寸不匹配的条件还在，下一帧会自己重来；这一帧先显示旧纹理。
    if (budget::isExhausted())
    {
        budget::noteDeferred();
        return;
    }

    DecodedImage rasterized = decodeSvgMemory(this->svgData.data(), this->svgData.size(), width, height);
    if (!rasterized.isValid())
    {
        Logger::warning("SVG 栅格化失败："
            + (rasterized.getError().empty() ? std::string("未知原因") : rasterized.getError()));
        return;
    }

    const TextureRef created =
        loadTextureFromPixels(rasterized.pixels.data(), rasterized.width, rasterized.height);
    if (!created.isValid())
        return;

    this->releaseOwnedTexture();
    this->texture         = created;
    this->ownsTexture     = true;
    this->svgRasterWidth  = width;
    this->svgRasterHeight = height;

    // 尺寸为 auto 时用第一次栅格化的结果定尺寸，避免退化成 0x0
    if (this->getWidth() <= 0.0f)
        this->setWidth((float)rasterized.width);
    if (this->getHeight() <= 0.0f)
        this->setHeight((float)rasterized.height);

    this->invalidate();
}

bool Image::applyAnimation(DecodedAnimation loaded)
{
    this->releaseOwnedTexture();

    // SVG 与动图互斥：换过来时把另一边的状态清掉
    this->svgData.clear();
    this->svgRasterWidth  = 0;
    this->svgRasterHeight = 0;

    this->animation        = std::move(loaded);
    this->animationElapsed = 0.0f;
    this->animationFrame   = 0;
    this->animationPlaying = true;
    // 后端的纹理仓库可能还没就绪（视图树常常早于 App::run()）：留给 update() 补上传
    this->animationPendingUpload = true;

    // 没有显式尺寸时用动图画布尺寸
    if (this->getWidth() <= 0.0f)
        this->setWidth((float)this->animation.width);
    if (this->getHeight() <= 0.0f)
        this->setHeight((float)this->animation.height);

    this->pendingPath.clear();
    this->invalidate();
    return true;
}

void Image::uploadAnimationFrame()
{
    if (this->animation.frames.empty() || !isTextureStoreReady())
        return;

    // 帧预算用完就推迟这一帧的动图上传：`animationPendingUpload`（或换帧标记）还在，
    // 下一帧会自己重来 —— 帧率掉了不好看，总比整帧被上传拖住好。
    if (budget::isExhausted())
    {
        budget::noteDeferred();
        return;
    }

    const size_t index = std::min(this->animationFrame, this->animation.frames.size() - 1);
    const std::vector<unsigned char>& frame = this->animation.frames[index];

    if (this->animationPendingUpload || !this->texture.isValid())
    {
        const TextureRef created =
            loadTextureFromPixels(frame.data(), this->animation.width, this->animation.height);
        if (!created.isValid())
            return;

        this->releaseOwnedTexture();
        this->texture                = created;
        this->ownsTexture            = true;
        this->animationPendingUpload = false;
        return;
    }

    // 换帧：就地更新，不重建纹理
    updateTexture(this->texture, frame.data(), this->animation.width, this->animation.height);
}

void Image::stepAnimation(float deltaSeconds)
{
    if (this->animation.frames.empty())
        return;

    if (deltaSeconds > 0.0f)
        this->animationElapsed += deltaSeconds;

    this->uploadAnimationFrame();

    const size_t index = frameIndexAt(this->animation, this->animationElapsed);
    if (index == this->animationFrame)
        return;

    this->animationFrame = index;
    this->uploadAnimationFrame();
}

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

    // 背压 3-3：解码启动走队列（并发受 getDecodeConcurrency() 限制）
    enqueueDecode(resolved, this->decodePriority, [this, generation, guard, resolved]() {
        task::runResult(
            [resolved] { return decodeImageFile(resolved.c_str()); },
        [this, generation, guard, resolved](DecodedImage image) {
            // 背压 3-4：解码一完成只登记"待上传"，真正的上传交给 pumpUploads()
            // 按 getMaxUploadsPerFrame() 每帧消费 -> 20 张完成会摊到多帧，不会一帧传 20 张
            notifyDecodeFinished(resolved, [this, generation, guard, image, resolved]() {
                if (guard.expired() || !guard.lock()->isAlive())
                    return; // 视图已经销毁，别再碰它

                if (generation != this->loadGeneration)
                    return; // 已经发起了更新的加载，这个结果丢掉

                if (!isTextureStoreReady())
                {
                    // 仓库还没就绪（视图树建得比 App::run 早）：让 update() 下一帧重新排队
                    this->asyncPending = false;
                    return;
                }
                if (!image.isValid())
                {
                    // 解码失败：允许有限次重试。以前同步那条路是每帧重试一次、每帧卡一次；
                    // 现在放开 asyncPending 让 update() 重新排队，超过次数就放弃并说清原因。
                    this->asyncPending = false;
                    this->loadRetries++;
                    if (this->loadRetries > kMaxLoadRetries)
                    {
                        Logger::warning("图片加载失败，放弃重试：" + resolved);
                        this->pendingPath.clear();
                    }
                    return;
                }

                this->asyncPending = false;
                this->loadRetries = 0;

                const TextureRef loaded =
                    loadTextureFromPixels(image.pixels.data(), image.width, image.height);
                if (!loaded.isValid())
                    return;

                releaseTexture(this->texture.index); // 换图：先放掉旧的（无效下标是安全 no-op）
                this->texture = loaded;
                retainTexture(loaded.index); // 静态图由路径缓存持有，这里只登记"有人在用"
                trimTextureCache(); // 2d：新纹理进缓存后收一次（超预算才淘汰，且只淘汰没人在用的）
                this->pendingPath.clear();

                if (this->getWidth() <= 0.0f)
                    this->setWidth(static_cast<float>(loaded.width));
                if (this->getHeight() <= 0.0f)
                    this->setHeight(static_cast<float>(loaded.height));

                this->invalidate();
            });
        });
    });
}

bool Image::setImageFromFile(const std::string& path)
{
    // 记下路径：仓库没就绪时这一句会失败，但要留着让 update() 重新排队。
    this->pendingPath = path;

    // 护栏：帧内同步加载会把这一帧直接卡住（IO + 解码 + 上传全在主线程）。
    // 批量/滚动场景请用 setImageFromFileAsync()；警告做节流，免得刷屏。
    if (budget::isFrameActive())
    {
        static double lastWarnSeconds = -1000.0;
        const double now = getSecondsNow();
        if (now - lastWarnSeconds > 2.0)
        {
            lastWarnSeconds = now;
            Logger::warning("在帧内同步加载图片（" + path
                + "）：这一帧会被 IO + 解码 + 上传卡住；批量/滚动场景请用 setImageFromFileAsync()");
        }
    }

    const TextureRef loaded = loadTexture(resourcePath(path));
    if (!loaded.isValid())
        return false;

    releaseTexture(this->texture.index); // 换图：先放掉旧的（无效下标是安全 no-op）


    this->texture = loaded;


    retainTexture(loaded.index); // 静态图由路径缓存持有，这里只登记"有人在用"
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
    // SVG：布局尺寸变了就重新栅格化（换窗口 / 改宽高 / uiScale 变化都会走这里）
    if (this->hasSvg())
        this->rasterizeSvg();

    // 动图：只在**可见**时推进帧时钟（离屏时不占上传带宽），回来按绝对时间对齐
    if (this->hasAnimation())
    {
        this->uploadAnimationFrame();

        if (this->animationPlaying && this->getAnimationFrameCount() > 1 && this->isVisibleInTree())
            this->stepAnimation(getFrameDeltaSeconds());
    }

    // 视图树通常在 App::run() 之前就建好了，那时图形后端还没创建、纹理传不上去。
    // 仓库就绪后**重新排一次异步加载**，而不是当场 loadTexture ——
    // 那条同步路正是"帧内卡住"的来源（评审"UI 永不卡"那一轮改掉的）。
    if (this->texture.isValid() || this->pendingPath.empty() || !isTextureStoreReady())
        return;
    if (this->asyncPending)
        return; // 已经在队列里：等解码完成后的上传回调即可

    this->setImageFromFileAsync(this->pendingPath);
}

void Image::setImage(const TextureRef& ref)
{
    this->texture = ref;
    this->invalidate();
}

ImagePlacement placeImage(ImageScalingType scaling, float imageWidth, float imageHeight, float boxWidth,
    float boxHeight, float scale, float offsetX, float offsetY)
{
    ImagePlacement out;
    if (imageWidth <= 0.0f || imageHeight <= 0.0f)
        return out;

    // 显示比例：默认 1.0（原比例）；非正数按 1.0 处理，别让图凭空消失
    const float ratio = (scale > 0.0f) ? scale : 1.0f;

    switch (scaling)
    {
        case ImageScalingType::STRETCH:
            out.width  = boxWidth * ratio;
            out.height = boxHeight * ratio;
            out.x      = (boxWidth - out.width) * 0.5f;
            out.y      = (boxHeight - out.height) * 0.5f;
            break;

        case ImageScalingType::FIT:
        {
            // 保持比例、完整放下（居中留边），再按显示比例放大/缩小
            const float fit = std::min(boxWidth / imageWidth, boxHeight / imageHeight) * ratio;
            out.width       = imageWidth * fit;
            out.height      = imageHeight * fit;
            out.x           = (boxWidth - out.width) * 0.5f;
            out.y           = (boxHeight - out.height) * 0.5f;
            break;
        }

        case ImageScalingType::FILL:
        {
            // 铺满 + 用 uv 裁切。缩放/平移在这里没有可解释的语义（裁切范围会跟着变），
            // 所以忽略；传了非默认值就提醒一次（只报一次，别刷屏）
            if (ratio != 1.0f || offsetX != 0.0f || offsetY != 0.0f)
            {
                static bool warned = false;
                if (!warned)
                {
                    warned = true;
                    BKLV_LOG_WARNING(
                        "scalingType=fill 时 scale / offsetX / offsetY 不生效（fill 靠 uv 裁切，"
                        "要缩放或平移请改用 fit / stretch / center）");
                }
            }
            const float fill   = std::max(boxWidth / imageWidth, boxHeight / imageHeight);
            const float shownW = boxWidth / (imageWidth * fill);
            const float shownH = boxHeight / (imageHeight * fill);
            out.width  = boxWidth;
            out.height = boxHeight;
            out.uvMinX = (1.0f - shownW) * 0.5f;
            out.uvMinY = (1.0f - shownH) * 0.5f;
            out.uvMaxX = 1.0f - out.uvMinX;
            out.uvMaxY = 1.0f - out.uvMinY;
            break;
        }

        case ImageScalingType::CENTER:
            // 原尺寸居中（不放大），显示比例乘在原尺寸上
            out.width  = imageWidth * ratio;
            out.height = imageHeight * ratio;
            out.x      = (boxWidth - out.width) * 0.5f;
            out.y      = (boxHeight - out.height) * 0.5f;
            break;
    }

    // 平移只对非 FILL 生效（FILL 的矩形就是整个盒子）
    if (scaling != ImageScalingType::FILL)
    {
        out.x += offsetX;
        out.y += offsetY;
    }
    return out;
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

    const ImagePlacement placement = placeImage(this->scalingType, imageWidth, imageHeight, width,
        height, this->displayScale, this->offsetX, this->offsetY);
    const ImVec2 destMin(x + placement.x, y + placement.y);
    const ImVec2 destMax(destMin.x + placement.width, destMin.y + placement.height);
    const ImVec2 uvMin(placement.uvMinX, placement.uvMinY);
    const ImVec2 uvMax(placement.uvMaxX, placement.uvMaxY);

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

void setImageScaleAttr(View* v, const ResolvedValue& r)
{
    static_cast<Image*>(v)->setDisplayScale(r.number);
}

void setImageOffsetXAttr(View* v, const ResolvedValue& r)
{
    Image* image = static_cast<Image*>(v);
    image->setOffset(r.number, image->getOffsetY());
}

void setImageOffsetYAttr(View* v, const ResolvedValue& r)
{
    Image* image = static_cast<Image*>(v);
    image->setOffset(image->getOffsetX(), r.number);
}

const AttributeTable& Image::attributes() const
{
    static const AttributeTable table = {
        { "src", AttributeKind::RESOURCE_PATH, setImageSrcAttr, nullptr },
        // Image 上的 cornerRadius 指**图片本身的圆角**（子类表优先于公共表里
        // 那个背景圆角），这才是 Image 上看得见的东西。
        { "cornerRadius", AttributeKind::NUMBER, setImageRadiusAttr, nullptr },
        { "scalingType", AttributeKind::ENUM, setScalingTypeAttr, "fit|fill|stretch|center" },
        // 显示比例（默认 1.0 = 原比例）、平移（像素）；fill 模式下无效（见 placeImage）
        { "scale", AttributeKind::NUMBER, setImageScaleAttr, nullptr },
        { "offsetX", AttributeKind::NUMBER, setImageOffsetXAttr, nullptr },
        { "offsetY", AttributeKind::NUMBER, setImageOffsetYAttr, nullptr },
        { "placeholderColor", AttributeKind::COLOR, setImagePlaceholderAttr, nullptr },
    };
    return table;
}

} // namespace bklv
