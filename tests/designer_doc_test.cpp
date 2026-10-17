// P1 验收：文档模型（保序/未知键）· 保存事务（校验先行/原子写/.bak）· 撤销栈 · 层操作。
// 无窗口，不需要图形后端。

#include <cstdio>
#include <string>

#include "designer/core/appearance_doc.hpp"
#include "designer/core/json_document.hpp"
#include "designer/core/save_transaction.hpp"
#include "designer/core/selection.hpp"
#include "designer/core/undo_stack.hpp"
#include "test_util.hpp"

using namespace designer;

namespace
{

// 故意混入：未知顶层键、$comment、未知层字段、非字母序的键 —— 保存时必须原样保留
const char* kDocText = R"({
  "$comment": "只给注释用",
  "layers": [
    { "op": "rect", "id": "bg", "fill": "#112233", "radius": 8, "zzUnknownField": [1, 2] },
    { "op": "text", "id": "label", "text": "设置", "fill": "#ffffff" }
  ],
  "futureTopLevelKey": { "keep": true },
  "states": { "focused": { "bg": { "fill": { "to": "#224466", "duration": 120 } } } }
})";

bool writeFile(const std::string& path, const std::string& text)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr)
        return false;
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
    return true;
}

std::string readFile(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr)
        return {};
    std::string out;
    char        buffer[512];
    size_t      n = 0;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0)
        out.append(buffer, n);
    std::fclose(f);
    return out;
}

bool fileExists(const std::string& path)
{
    return std::fopen(path.c_str(), "rb") != nullptr;
}

} // namespace

int main()
{
    std::printf("== 1. 读入 → 改一个字段 → 写回：未知键与键序逐字节保留 ==\n");
    {
        AppearanceDoc doc;
        std::string   error;
        check(doc.fromText(kDocText, error), "解析成功（" + error + "）");

        check(doc.get("/$comment") != nullptr, "$comment 被保留");
        check(doc.get("/futureTopLevelKey/keep") != nullptr, "未知顶层键被保留");
        check(doc.get("/layers/0/zzUnknownField") != nullptr, "未知层字段被保留");

        check(doc.setLayerField("bg", "radius", bklv::Json(12), error), "改 radius 成功（" + error + "）");
        const std::string saved = doc.serialize();
        check(saved.find("\"zzUnknownField\"") != std::string::npos, "保存后未知层字段还在");
        check(saved.find("futureTopLevelKey") != std::string::npos, "保存后未知顶层键还在");
        check(saved.find("\"$comment\"") != std::string::npos, "保存后 $comment 还在");
        check(saved.find("\"radius\": 12") != std::string::npos, "radius 改成 12");
        // 键序：$comment 仍在 layers 之前、futureTopLevelKey 仍在 states 之前
        check(saved.find("$comment") < saved.find("layers"), "键序保留了 $comment 在前");
        check(saved.find("\"layers\"") < saved.find("futureTopLevelKey"), "键序保留了 layers 在未知键前");
        check(doc.isDirty(), "改过之后是脏的");
        check(doc.setLayerField("bg", "radius", bklv::Json(8), error), "改回原值");
        check(!doc.isDirty(), "改回原值后不再算脏（按加载时的文本比较）");
        doc.markSaved();
        check(!doc.isDirty(), "markSaved 之后不脏");
    }

    std::printf("\n== 2. 层操作：增 / 删 / 移动 / 复制 / id 冲突 ==\n");
    {
        AppearanceDoc doc;
        std::string   error;
        doc.fromText(kDocText, error);

        std::string newId;
        check(doc.addLayer("shadow", newId, error), "加一层 shadow（" + error + "）");
        checkEqual(newId, std::string("shadow0"), "自动 id = shadow0");
        checkEqualSize(doc.getLayers().size(), (size_t)3, "现在三层");
        check(doc.addLayer("rect", newId, error, "bg") == false, "重复 id 被拒绝");
        check(error.find("已存在") != std::string::npos, "错误说明是 id 冲突");

        check(doc.moveLayer("shadow0", 0, error), "移到最前（" + error + "）");
        checkEqual(doc.getLayers()[0].id, std::string("shadow0"), "顺序：shadow0 在第一位");
        checkEqual(doc.getLayers()[1].id, std::string("bg"), "bg 退到第二位");

        std::string copyId;
        check(doc.duplicateLayer("bg", copyId, error), "复制 bg（" + error + "）");
        checkEqual(copyId, std::string("bg_copy"), "复制出的 id = bg_copy");
        check(doc.get("/layers/" + std::to_string(doc.findLayer(copyId)) + "/zzUnknownField") != nullptr,
            "复制出来的层带着未知字段");

        check(doc.removeLayer("bg_copy", error), "删掉复制层");
        checkEqualSize(doc.getLayers().size(), (size_t)3, "回到三层");
        check(doc.removeLayer("nope", error) == false, "删不存在的层 -> 失败");
    }

    std::printf("\n== 3. 保存事务：校验失败不落盘、成功留 .bak、原子写 ==\n");
    {
        const std::string path = "dist/designer_doc_test.json";
        std::remove(path.c_str());
        std::remove((path + ".bak").c_str());
        std::remove((path + ".tmp").c_str());

        check(writeFile(path, "{\n  \"v\": 1\n}\n"), "先放一份旧文件");

        std::string error;
        SaveOptions options;
        options.validate = [](const std::string&, std::string& why) {
            why = "故意让它失败";
            return false;
        };
        const SaveResult rejected = saveText(path, "{\n  \"v\": 2\n}\n", options, error);
        check(rejected == SaveResult::VALIDATION_FAILED, "校验失败 -> VALIDATION_FAILED");
        checkEqual(readFile(path), std::string("{\n  \"v\": 1\n}\n"), "原文件没被改");
        check(!fileExists(path + ".bak"), "校验失败时不留 .bak");
        check(!fileExists(path + ".tmp"), "校验失败时不留 .tmp");

        options.validate = [](const std::string&, std::string&) { return true; };
        const SaveResult ok = saveText(path, "{\n  \"v\": 2\n}\n", options, error);
        check(ok == SaveResult::OK, "校验通过 -> OK（" + error + "）");
        checkEqual(readFile(path), std::string("{\n  \"v\": 2\n}\n"), "新内容落盘");
        checkEqual(readFile(path + ".bak"), std::string("{\n  \"v\": 1\n}\n"), ".bak 是旧内容");
        check(!fileExists(path + ".tmp"), "成功后不留 .tmp");

        // 外部修改检查
        SaveOptions guarded;
        guarded.expectedMtime = fileMtime(path) - 100; // 假装编辑期间文件被改过
        const SaveResult modified = saveText(path, "{\n  \"v\": 3\n}\n", guarded, error);
        check(modified == SaveResult::EXTERNALLY_MODIFIED, "外部修改 -> EXTERNALLY_MODIFIED");
        guarded.force = true;
        check(saveText(path, "{\n  \"v\": 3\n}\n", guarded, error) == SaveResult::OK,
            "force 时照样写");
        std::remove(path.c_str());
        std::remove((path + ".bak").c_str());
    }

    std::printf("\n== 4. 撤销栈：同 key 合并、跨 key 新增、撤销/重做、上限 ==\n");
    {
        UndoStack stack(3);
        // 连续改同一个字段（模拟拖动）：500ms 内只留一步
        stack.record("v0", "改 radius", "radius", 1000);
        stack.record("v1", "改 radius", "radius", 1200);
        stack.record("v2", "改 radius", "radius", 1600);
        checkEqualSize(stack.undoCount(), (size_t)1, "同一字段连续改只留一步");
        stack.record("v3", "改 fill", "fill", 2000);
        checkEqualSize(stack.undoCount(), (size_t)2, "换个字段 -> 新的一步");

        std::string restored;
        check(stack.undo("v4", restored), "能撤销");
        checkEqual(restored, std::string("v3"), "撤销回到改 fill 之前");
        check(stack.canRedo(), "撤了之后可重做");
        check(stack.undo("v3", restored) && restored == "v0", "再撤回到最原始");

        check(stack.redo("v0", restored) && restored == "v3", "重做回到 v3");
        checkEqualSize(stack.undoCount(), (size_t)1, "重做后再撤销步数正确");

        // 上限：超过 3 步丢最旧的
        UndoStack small(3);
        for (int i = 0; i < 6; i++)
            small.record("s" + std::to_string(i), "x", "k" + std::to_string(i), 10000 + i * 1000);
        checkEqualSize(small.undoCount(), (size_t)3, "上限 3 步");

        stack.clear();
        check(!stack.canUndo() && !stack.canRedo(), "clear 之后都空");
    }

    std::printf("\n== 5. 选中：三处界面共用的同一份值 ==\n");
    {
        Selection a;
        check(!a.valid(), "默认未选中");
        a.kind    = SelectionKind::APPEARANCE;
        a.file    = "widget/button.text";
        a.pointer = "/layers/1";
        a.layerId = "label";
        check(a.valid(), "填好之后有效");

        Selection b = a;
        check(a == b, "拷贝相等");
        b.pointer = "/layers/0";
        check(a != b, "路径不同即不等");
        check(!Selection::none().valid(), "none() 无效");
    }

    std::printf("\n== 6. 源码应用失败时文档不动（P5 验收）==\n");
    {
        AppearanceDoc doc;
        std::string   error;
        check(doc.fromText(kDocText, error), "先装一份好的");
        const std::string before = doc.serialize();

        // 语法坏：fromText 失败，且**不能**把旧文档换掉
        check(!doc.fromText("{ \"layers\": [ ", error), "坏 JSON -> 解析失败");
        check(!error.empty(), "错误里有原因（含行列）");
        checkEqual(doc.serialize(), before, "失败后文档仍是上一份（不丢用户编辑）");

        // 能解析但结构不对：fromText 成功（顶层对象），后续 apply 由编译校验兜
        check(doc.fromText("{\"layers\": []}", error), "空 layers 能解析");
        checkEqualSize(doc.getLayers().size(), (size_t)0, "换成空 layers");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
