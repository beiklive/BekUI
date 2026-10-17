#pragma once

// 外观文件的语义操作：层 / 状态 / 过渡。底层仍是 JsonDocument 的路径补丁，
// 所以未知键与键序不会丢。

#include <string>
#include <vector>

#include "designer/core/json_document.hpp"

namespace designer
{

struct LayerInfo
{
    std::string id;
    std::string op;
    size_t      index = 0; // 在 JSON 数组里的位置（书写顺序）
};

class AppearanceDoc : public JsonDocument
{
  public:
    std::vector<LayerInfo> getLayers() const;
    // 按 id 找层的下标；没有返回 -1
    int findLayer(const std::string& id) const;

    // 新增一层：op 必填，id 省略时自动生成 "<op><n>" 且保证不与现有 id 冲突
    bool addLayer(const std::string& op, std::string& outId, std::string& error,
        const std::string& id = {});
    bool removeLayer(const std::string& id, std::string& error);
    bool moveLayer(const std::string& id, size_t toIndex, std::string& error);
    bool duplicateLayer(const std::string& id, std::string& outId, std::string& error);
    bool setLayerField(const std::string& id, const std::string& key, designer::Json value,
        std::string& error);
    bool removeLayerField(const std::string& id, const std::string& key, std::string& error);

    std::vector<std::string> getStateNames() const;

  private:
    // 层所在的数组指针路径：/layers/<i>
    bool layerPointer(const std::string& id, std::string& outPointer) const;
};

} // namespace designer
