// 视图文档模型：节点路径 / 属性读写（P6 第二半与视图编辑的地基）。

#include <cstdio>
#include <string>

#include "designer/core/view_doc.hpp"
#include "test_util.hpp"

using namespace designer;

int main()
{
    std::printf("== 1. 节点路径：深度优先、根为空指针 ==\n");
    {
        ViewDoc doc;
        std::string error;
        check(doc.fromText(R"({
  "type": "Box", "axis": "row",
  "children": [
    { "type": "Label", "text": "标题" },
    { "type": "Box", "children": [ { "type": "Image", "src": "@res/a.png" } ] }
  ]
})", error), "解析（" + error + "）");

        const std::vector<ViewNodeInfo> nodes = doc.listNodes();
        checkEqualSize(nodes.size(), (size_t)4, "四个节点");
        checkEqual(nodes[0].pointer, std::string(), "根指针为空");
        checkEqual(nodes[1].pointer, std::string("/children/0"), "第一个子节点");
        checkEqual(nodes[3].pointer, std::string("/children/1/children/0"), "孙节点");
        checkEqual(nodes[3].type, std::string("Image"), "孙节点类型");
        checkEqualInt(nodes[3].depth, 2, "孙节点深度 2");
    }

    std::printf("\n== 2. 属性读写走路径补丁 ==\n");
    {
        ViewDoc doc;
        std::string error;
        doc.fromText(R"({"type":"Label","text":"a","zzKeep":1})", error);
        check(doc.setAttribute("", "text", designer::Json("b"), error), "改根节点 text");
        checkEqual(doc.attributeText("", "text"), std::string("b"), "读回 b");
        check(doc.get("/zzKeep") != nullptr, "未知键保留");
        check(doc.removeAttribute("", "text", error), "删 text");
        check(doc.attributeText("", "text").empty(), "删后为空");
        check(!doc.removeAttribute("", "nope", error), "删不存在的键 -> 失败");
    }

    std::printf("\n== 3. 真实视图文件能列出来 ==\n");
    {
        ViewDoc doc;
        std::string error;
        check(doc.loadFile("resources/theme/default/view/page_json.json", error),
            "装载 page_json（" + error + "）");
        const std::vector<ViewNodeInfo> nodes = doc.listNodes();
        check(nodes.size() > 3, "节点数合理（" + std::to_string(nodes.size()) + "）");
        check(!nodes[0].type.empty(), "根节点有 type");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
