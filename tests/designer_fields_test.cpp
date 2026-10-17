// P3 验收（第一半）：检视器字段由**图层注册表**派生 —— op 加参数，设计器自动多一个字段。

#include <cstdio>
#include <string>

#include "bklv/compile/layer_registry.hpp"
#include "designer/core/appearance_doc.hpp"
#include "designer/core/layer_fields.hpp"
#include "test_util.hpp"

using namespace designer;

int main()
{
    std::printf("== 1. 每个已注册的 op 都能派生出字段 ==\n");
    const std::vector<std::string> ops = { "rect", "text", "image", "shadow", "glass", "custom" };
    for (const std::string& op : ops)
    {
        const bklv::compile::LayerType* type = bklv::compile::findLayer(op);
        check(type != nullptr, op + " 已注册");
        if (type == nullptr)
            continue;

        const std::vector<FieldDesc> fields = layerFields(op);
        check(!fields.empty(), op + " 派生出了字段");
        // 注册表声明的每个参数都必须出现
        int missing = 0;
        for (const bklv::compile::LayerParam& param : type->params)
        {
            bool found = false;
            for (const FieldDesc& field : fields)
                found = found || field.key == param.name;
            if (!found)
            {
                missing++;
                std::printf("    缺字段：%s / %s\n", op.c_str(), param.name.c_str());
            }
        }
        checkEqualInt(missing, 0, op + " 的注册参数全部出现在字段表里");
        // 固定字段里 inset / offset 必须有（装饰件定位靠它们）
        bool hasInset = false;
        for (const FieldDesc& field : fields)
            hasInset = hasInset || field.key == "inset";
        check(hasInset, op + " 有 inset 字段");
    }

    std::printf("\n== 2. 未注册的 op 返回空表 ==\n");
    check(layerFields("__nope__").empty(), "未知 op -> 空字段表");

    std::printf("\n== 3. 字段读写真的落到 JSON 上（round-trip）==\n");
    {
        AppearanceDoc doc;
        std::string  error;
        check(doc.fromText(R"({"layers":[{"op":"rect","id":"bg","radius":4}]})", error), "解析");
        check(doc.setLayerField("bg", "radius", designer::Json(12), error), "写 radius");
        const designer::Json* value = doc.get("/layers/0/radius");
        check(value != nullptr && value->is_number(), "读回来是数字");
        if (value != nullptr)
            checkFloat(value->get<float>(), 12.0f, "值 = 12");
        check(doc.removeLayerField("bg", "radius", error), "删字段");
        check(doc.get("/layers/0/radius") == nullptr, "删之后不存在（不是写 0）");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
