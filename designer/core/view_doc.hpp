#pragma once

// 视图描述文件（view/*.json）的文档模型：只做设计器需要的语义 —— 节点路径与属性读写。
// 节点的"指针"就是它 children 的 JSON Pointer（根节点是空指针），和选中模型一致。

#include <string>
#include <vector>

#include "designer/core/json_document.hpp"

namespace designer
{

struct ViewNodeInfo
{
    std::string pointer;  // 根 = ""，子节点 = "/children/0/children/1"
    std::string type;     // 节点 type（空 = 没写，非法）
    int         depth = 0;
};

class ViewDoc : public JsonDocument
{
  public:
    // 深度优先列出全部节点（含根）。超过 maxDepth 的截断（视图解析上限 32）。
    std::vector<ViewNodeInfo> listNodes(int maxDepth = 32) const;

    bool setAttribute(const std::string& pointer, const std::string& key, Json value,
        std::string& error);
    bool removeAttribute(const std::string& pointer, const std::string& key, std::string& error);

    // 节点属性的原样文本（用于检视器显示；不存在返回空）
    std::string attributeText(const std::string& pointer, const std::string& key) const;
};

} // namespace designer
