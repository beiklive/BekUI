#pragma once

// 外观驱动的控件基类：绘制结构与状态动效全部来自主题里的 JSON（core/appearance.hpp）。
//
// 分工（这是 L3 粒度的边界）：
//   - **装饰层**（底、边框、文字、图片、状态动效）由 JSON 描述，本类负责解释并绘制
//   - **语义性绘制**（文本换行/截断、图片缩放、进度弧、滑块轨道…）走 custom 层，
//     由具体控件重写 drawCustomLayer；把它们也塞进 JSON 等于写绘制解释器，
//     性能和可调试性都会明显变差
//
// 数据来源（`"source": "text"` 之类）由子类翻译成实际内容，见 getLayerText/getLayerImage。

#include <string>
#include <vector>

#include "bklv/core/appearance.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/graphics.hpp" // TextureRef

namespace bklv
{

class AppearanceView : public Box
{
  public:
    // appearanceName 是主题里 widgets/<name>.json 的名字（如 "button"）。
    explicit AppearanceView(std::string appearanceName, bool focusable);

    // 文本层的数据（source = "text"）。也参与按钮这类控件的自动测量。
    void setText(const std::string& text);
    const std::string& getText() const { return this->text; }

    // 换一份外观文件（如 "card" -> widgets/rectangle.card.json）。
    // 传空串表示用控件默认的那份。
    void setAppearanceVariant(const std::string& variant);

    // 状态：focused / pressed / disabled / selected / hover。
    // 打开状态会让外观里的对应动效跑起来（tweeny 插值）。
    void setAppearanceState(const std::string& state, bool active);
    bool isAppearanceStateActive(const std::string& state) const;

    bool isFocusable() const override { return this->focusable; }
    void onFocusGained() override;
    void onFocusLost() override;

    void update() override;
    void applyTheme() override;

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, FrameContext* ctx) override;

    // layer.source 的取值。默认：source == "text" 时返回 setText() 的内容。
    virtual std::string getLayerText(const std::string& source);
    // layer.source 的取值（图片）。默认无效引用。
    virtual TextureRef getLayerImage(const std::string& source);
    // 这一层用哪个字体。默认正文字体；图标层要返回 getIconFont()
    // （私有区字形不在正文字体里，用错字体会量出错误的尺寸、画出方块）。
    virtual ImFont* getLayerFont(const AppearanceLayer& layer);
    // custom 层：交给子类画（矩形范围已按 inset/offset 算好）。
    virtual void drawCustomLayer(ImDrawList*, const AppearanceLayer&, float, float, float, float,
        const AppearanceInstance&)
    {
    }

    // 开启"按内容算固有尺寸"：文字层用字号量文本 + 两侧 inset。
    //
    // **只能给叶子控件开**：Yoga 不允许带测量函数的节点再有子节点
    // （DialogPanel 这类要装按钮行的容器就不能开，得用自己的尺寸）。
    void enableContentMeasurement();

    const AppearanceInstance& getAppearanceInstance() const { return this->appearance; }
    const std::string& getAppearanceName() const { return this->appearanceName; }

  public:
    // 当前生效的外观（只读）。测试与诊断用：换肤后这里应该换成新主题的那份。
    const Appearance& getActiveAppearance() const { return this->appearance.getAppearance(); }

    // 按层 id 找索引（-1 表示没有）。滑块这类控件用它按名字取"轨道/进度/滑块"各段的颜色。
    int indexOfLayer(const std::string& layerId) const
    {
        return this->appearance.getAppearance().indexOfLayer(layerId);
    }
    const AppearanceInstance& getAppearanceInstanceForTest() const { return this->appearance; }

  protected:

  private:


    // 取层在给定控件矩形里的实际绘制范围（应用 inset 与 offset）
    void layerRect(const AppearanceLayer& layer, float x, float y, float width, float height,
        float out[4]) const;

    std::string appearanceName; // 默认外观名（不含 variant）
    std::string variant;        // 当前 variant；空表示用默认
    std::string text;
    bool focusable = false;
    bool pressed   = false;
    AppearanceInstance appearance;
};

} // namespace bklv
