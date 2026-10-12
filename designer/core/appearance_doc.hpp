#pragma once

// 一份外观 JSON 的可编辑模型。
//
// 用 **保序 JSON**（ordered_json）整棵留在内存里，改哪个键就改哪个键，
// 没碰过的键原样写回去 —— 所以"读 → 改 → 写"不会丢字段、不会乱键序。
// 路径写法（只用于 box / layers 这类简单结构）：
//   box.height        layers[0].fill        layers[2].inset
// states 的通道名里带点（"bg.fill"），不走路径，直接操作 json。

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace designer
{

// 数值写成 JSON：整数就写整数（避免 64 → 64.0 的无意义改动）
nlohmann::ordered_json numberValue(double value);

// 按插值属性名给一个合理的默认目标值：
// 颜色类属性给颜色串（给 0.0 的话外观会编译失败——颜色通道不接受数字），
// 尺寸类给数字。状态通道默认值用它，省得用户加完通道先撞一次编译错误。
nlohmann::ordered_json defaultChannelValue(const std::string& propertyName);

class AppearanceDoc
{
  public:
    using Json = nlohmann::ordered_json;

    // 解析失败返回 false + error（不改动原有内容）
    bool parse(const std::string& jsonText, std::string& error);

    // 2 空格缩进；与原文件风格一致
    std::string serialize() const;

    const Json& root() const { return this->doc; }
    Json& root() { return this->doc; }

    bool hasObject(const std::string& key) const;
    bool hasArray(const std::string& key) const;

    // ---- box ----
    bool getBoxNumber(const std::string& key, double& out) const;
    void setBoxNumber(const std::string& key, double value);

    // 改 box 里数组字段的某一项（padding / margin 的四值写法）。
    // 不是数组、或下标越界就什么也不做。
    void setBoxArrayElement(const std::string& key, size_t index, double value);
    bool getBoxString(const std::string& key, std::string& out) const;
    void setBoxString(const std::string& key, const std::string& value);
    void removeBoxKey(const std::string& key);
    std::vector<std::string> boxKeys() const;

    // ---- layers ----
    int layerCount() const;
    const Json* layer(int index) const;
    Json* layer(int index);
    std::string layerId(int index) const;
    std::string layerOp(int index) const;
    bool getLayerValue(int index, const std::string& key, Json& out) const;
    void setLayerValue(int index, const std::string& key, const Json& value);
    void removeLayerKey(int index, const std::string& key);
    // 新增一层（按常用字段给一份合理的默认），返回新下标
    int addLayer(const std::string& op);
    void removeLayer(int index);
    bool moveLayer(int index, int delta);       // delta = -1 上移 / +1 下移
    int duplicateLayer(int index);

    // 追加一份层（用于跨外观粘贴）。会自动把 id 改成唯一的（`bg` → `bg2`），
    // 返回新层的下标；传进来的不是对象则返回 -1。
    int appendLayer(const nlohmann::ordered_json& layer);

    // 把某一层的某个字段的值套到其它层（多个层要设同一套参数时用）。
    // 目标层没有这个字段就新加；返回真正改动的层数。源层自己会被跳过。
    int applyFieldToLayers(int sourceIndex, const std::string& field,
        const std::vector<int>& targets);

    // ---- states ----
    std::vector<std::string> stateNames() const;
    int channelCount(const std::string& state) const;
    void addState(const std::string& state);
    void removeState(const std::string& state);
    void addChannel(const std::string& state, const std::string& channel);
    void removeChannel(const std::string& state, int index);

    // 层序硬约定（hlBg 在内容层前、shadow 在底色前、hlGlow/hlBorder 最后）违反时给出提示
    std::vector<std::string> layerOrderWarnings() const;

    // 按上面的约定自动重排（返回 true 表示真的动过）。
    // 顺序：shadow → 底色 rect → hlBg → 内容(text/image/custom) → 其他 rect → hlGlow/hlBorder
    bool reorderLayersByConvention();

  private:
    Json doc = Json::object();
};

} // namespace designer
