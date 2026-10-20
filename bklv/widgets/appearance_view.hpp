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

#include <array>
#include <map>
#include <string>
#include <vector>

#include "bklv/core/appearance.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/graphics.hpp" // TextureRef
#include "bklv/core/time.hpp"     // Animatable（聚焦放大）

namespace bklv
{

// 图标层用哪套图标字体。库里有两套：
//   MATERIAL —— bklv 自带的 Material Icons（icon::font()），控件图标默认用它；
//   SWITCH   —— switch_icons.ttf（按键/系统图标），只给提示栏、菜单这类地方用。
enum class IconFontKind
{
    MATERIAL,
    SWITCH,
};

class AppearanceView : public Box
{
  public:
    // 调试层按这个显示类型名；不重写就只剩基类的 "bklv::View"，排查视图树时没有类型信息。
    std::string getClassString() const override { return "bklv::AppearanceView"; }

    // 内容槽（text / icon / info）也能从视图 JSON 写：图标字体那套的 `@icon/名字` 由
    // core/attributes.cpp 的 TEXT 解析统一处理，子类不用各写一份。
    const bklv::AttributeTable& attributes() const override;

    // 语义绘制交给控件自己（CUSTOM 层）；**public** 是为了让注册表里的绘制钩子能调它
    virtual void drawCustomLayer(ImDrawList*, const AppearanceLayer&, float, float, float, float,
        const AppearanceInstance&)
    {
    }

    // 开启"按内容算固有尺寸"：文字层用字号量文本 + 两侧 inset。
    //
    // **只能给叶子控件开**：Yoga 不允许带测量函数的节点再有子节点
    // （DialogPanel 这类要装按钮行的容器就不能开，得用自己的尺寸）。
    void enableContentMeasurement();

    // appearanceName 是主题里 widgets/<name>.json 的名字（如 "button"）。
    explicit AppearanceView(std::string appearanceName, bool focusable);

    // ---- 内容槽：可无限添加，按 id 索引 ----
    // 外观里的 `"source": "<id>"` 就是槽 id；text / icon / info / image 是沿用的默认 id。
    void setText(const std::string& id, const std::string& value);
    void setIcon(const std::string& id, const std::string& glyph);
    void setImage(const std::string& id, const TextureRef& texture);
    void clearSlot(const std::string& id);
    bool hasSlot(const std::string& id) const;
    const std::string& getSlotText(const std::string& id) const;

    static const bklv::AttributeTable& appearanceAttributes();


    // 换一份外观文件（如 "card" -> widgets/rectangle.card.json）。
    // 传空串表示用控件默认的那份。
    void setAppearanceVariant(const std::string& variant);

    // 换基名（默认由构造函数给）。名字 = 主题里的路径名（"base/button/button"、"widget/button"）。
    // 同一个控件类要按不同"组合外观"出场时用它，比给每个变体建子类省事。
    void setAppearanceName(const std::string& name);

    // 状态：focused / pressed / disabled / selected / hover。
    // 打开状态会让外观里的对应动效跑起来（tweeny 插值）。
    void setAppearanceState(const std::string& state, bool active);

    // 状态里的视图级通道（`states.<状态>.self`）显式推进：无窗口测试用；
    // 正常运行由 update() 用帧间隔调用。
    void stepStateViewChannels(float deltaSeconds);
    bool isAppearanceStateActive(const std::string& state) const;

    bool isFocusable() const override { return this->focusable; }
    void onFocusGained() override;
    void onFocusLost() override;

    void update() override;
    void applyTheme() override;

    // ---- 组合变体用的内容槽（外观里 source = "icon" / "info" 的层显示它们）----
    //
    // 图标是图标字体的字形；信息文字放右侧（下拉框当前值、开关的"开/关"、选择器数值…）。
    // 放在基类是因为这些组合外观不只 Button 用（Dropdown 也能换上去）。


    void setIconFontKind(IconFontKind kind) { this->iconFontKind = kind; }
    IconFontKind getIconFontKind() const { return this->iconFontKind; }

    // 两套图标字体的统一入口：MATERIAL -> icon::font()，SWITCH -> getIconFont()。
    // 返回 nullptr 表示那套还没装好（调用方自己回退正文字体）。
    static ImFont* iconFontFor(IconFontKind kind);

    // ---- 按层指定用哪套图标字体（不指定 = 跟随 setIconFontKind 的那一套）----
    // 用途：同一个控件里"图标层用 Material、箭头层用 switch"这种混排，
    // 也不用为此写一个 getLayerFont 重写 —— 而且运行时可切，方便调。
    void setLayerIconFontKind(const std::string& layerId, IconFontKind kind);
    void clearLayerIconFontKind(const std::string& layerId);
    bool hasLayerIconFontKind(const std::string& layerId) const;
    IconFontKind getLayerIconFontKind(const std::string& layerId) const;

    // 直接套一份**外部的**外观 JSON（用当前主题解析）。
    // 设计器靠它做"改了但还没保存也能看到效果"：编译 → 换成这一份 → 重排。
    // 失败返回 false + error（原外观不变），成功时也会套用其中的 box 段。
    bool applyAppearanceJson(const std::string& json, std::string& error);

    // 第 index 层在"按 inset 收缩后的控件盒"里的矩形（相对盒子左上角）。工具/编辑器做点选时用；
    // 返回 false = 下标越界。x/y/width/height 传盒子的绝对位置与尺寸。
    bool getLayerRect(int index, float x, float y, float width, float height, float outRect[4]) const;

    // ---- 设计期叠加（默认关闭，运行时零影响）----
    // 选中框：-1 = 不画；索引越界按"不画"处理（不崩）。
    void setDesignSelection(int layerIndex, ImU32 color = 0xFFD9822B);
    int  getDesignSelection() const { return this->designSelection; }
    // 画板边框（沿控件外框描一圈）：设计器用它标出"这是一块画板"
    void setDesignGuides(bool on, ImU32 color = 0x66FFFFFF);
    bool getDesignGuides() const { return this->designGuides; }


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

    const AppearanceInstance& getAppearanceInstance() const { return this->appearance; }
    // 视图过渡的取用点（注意：上面那个 const 版是另一件事，别混）
    AppearanceInstance* getTransitionAppearanceInstance() override { return &this->appearance; }
    const Appearance* getTransitionAppearance() const override
    {
        // 没绑定时 getAppearance() 给一份空外观（hasTransitions() 为 false），所以直接给地址
        return &this->appearance.getAppearance();
    }
    const std::string& getAppearanceName() const { return this->appearanceName; }

  public:
    // 当前生效的外观（只读）。测试与诊断用：换肤后这里应该换成新主题的那份。
    const Appearance& getActiveAppearance() const { return this->appearance.getAppearance(); }

    // 聚焦框形状：基类给世界矩形，这里补上自己的圆角半径 ——
    // 所以"聚焦到圆形控件上就变圆"是自动的（外观里的 radius 已经写成大圆角了）。
    focus_ring::Outline getFocusOutline() const override;

    // 当前外观的圆角半径（取 bg 层；没有 bg 层就是 0）
    float getCornerRadius() const;

    // 聚焦时轻微放大（画框由浮层负责，控件这边只做"变大一点"的反馈）。
    // 数值写死在代码里（见 kFocusScale* 常量），全屏级的大面板可以自己关掉。
    void setFocusScaleEnabled(bool enabled) { this->focusScaleEnabled = enabled; }
    bool isFocusScaleEnabled() const { return this->focusScaleEnabled; }
    void setFocusScale(float scale) { this->focusScaleTarget = scale > 0.0f ? scale : 1.0f; }
    float getFocusScale() const { return this->focusScaleTarget; }

    // 按层 id 找索引（-1 表示没有）。滑块这类控件用它按名字取"轨道/进度/滑块"各段的颜色。
    int indexOfLayer(const std::string& layerId) const
    {
        return this->appearance.getAppearance().indexOfLayer(layerId);
    }
    const AppearanceInstance& getAppearanceInstanceForTest() const { return this->appearance; }

    // 外观还在补间中？（想等动效播完再摘掉/切换的地方用这个）
    bool isAppearanceAnimating() const { return this->appearance.isAnimating(); }

  protected:

  private:
    int   designSelection = -1;
    ImU32 designSelectionColor = 0;
    bool  designGuides = false;
    ImU32 designGuidesColor = 0;

    // 聚焦放大：每帧按焦点变化推进一条补间
    void updateFocusScale();


    // 取层在给定控件矩形里的实际绘制范围（应用 inset 与 offset）
    void layerRect(const AppearanceLayer& layer, float x, float y, float width, float height,
        float out[4]) const;

    std::string appearanceName; // 默认外观名（不含 variant）
    std::string variant;        // 当前 variant；空表示用默认
    struct ContentSlot
    {
        std::string id;
        enum class Kind
        {
            TEXT,
            ICON,
            IMAGE,
        };
        Kind        kind = Kind::TEXT;
        std::string text;
        TextureRef  image;
    };

    // 槽数量很小（一般 < 10），线性查找比哈希更快、零分配
    ContentSlot*       findSlot(const std::string& id);
    const ContentSlot* findSlot(const std::string& id) const;

    std::vector<ContentSlot> contentSlots;
    bool focusable = false;
    IconFontKind iconFontKind = IconFontKind::MATERIAL;
    std::map<std::string, IconFontKind> layerIconFontKinds;

    // 状态驱动的视图级通道（translate/scale/alpha/rotate）。逐通道一根弹簧式补间，
    // 最后激活的状态赢（和层通道"先撤旧补间再建新的"语义一致）。
    std::array<Animatable, (size_t)ViewChannel::COUNT> stateViewAnims;
    std::array<bool, (size_t)ViewChannel::COUNT> stateViewDriven{};
    std::vector<std::string> activeStateOrder;
    bool stateViewInit = false;
    void refreshStateViewChannels();
    bool pressed   = false;
    AppearanceInstance appearance;

    // 聚焦放大：一条从 1.0 到 focusScaleTarget 的补间（用库自带的 Animatable）
    Animatable focusScaleAnim;
    float focusScaleTarget        = 1.05f;
    float focusScaleApplied       = 1.0f;
    bool  focusScaleEnabled       = true;
    bool  focusScaleInitialized   = false;
    bool  focusScaleWasFocused    = false;
};

} // namespace bklv
