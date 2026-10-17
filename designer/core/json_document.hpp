#pragma once

// 设计器的文档模型：一份 JSON 文件的**保序、可路径读写、原文可回写**的表示。
//
// 三条硬约束（DESIGN.md §6.1）：
//   ① 保序：用 nlohmann::ordered_json，键顺序按文件里的顺序保留；
//   ② 未知键原样保留：所有修改都是"路径补丁"，不重新构造对象 —— 拼错的键、$comment
//      这类元数据、将来新增的字段，保存后逐字节还在；
//   ③ 读失败/写失败都有明确原因（不抛异常：库的约定是不让异常穿过公开 API）。

#include <string>

#include <nlohmann/json.hpp>

// 库的 `designer::Json` 是 nlohmann::json（std::map）—— dump 时**键按字母重排**，
// 而仓库里的主题 JSON 是手写顺序（有语义）。设计器要保住这个顺序，所以自己用
// ordered_json；交给库的时候一律用字符串（库的入口本来就吃 text）。
namespace designer { using Json = nlohmann::ordered_json; }
#include "bklv/compile/json_util.hpp"

namespace designer
{

// JSON Pointer 子集：`/layers/2/fill`（只支持对象键与数组下标，`~0`/`~1` 转义也支持）
struct Pointer
{
    std::vector<std::string> tokens;

    static Pointer parse(const std::string& text);
    std::string toString() const;
    bool empty() const { return this->tokens.empty(); }
    // 去掉最后一段（父路径）；已经是根时返回空 Pointer
    Pointer parent() const;
    const std::string& back() const { return this->tokens.back(); }
};

class JsonDocument
{
  public:
    bool loadFile(const std::string& path, std::string& error);
    bool fromText(const std::string& text, std::string& error);

    const std::string& getPath() const { return this->filePath; }
    const designer::Json& root() const { return this->document; }
    designer::Json& root() { return this->document; }

    // 读：找不到返回 nullptr（区分"值是 null"与"键不存在"）
    const designer::Json* get(const Pointer& pointer) const;
    const designer::Json* get(const std::string& pointerText) const;

    // 写：父路径必须存在（数组下标允许 == size，表示追加）；失败填 error
    bool set(const Pointer& pointer, designer::Json value, std::string& error);
    bool set(const std::string& pointerText, designer::Json value, std::string& error);
    bool remove(const Pointer& pointer, std::string& error);
    bool remove(const std::string& pointerText, std::string& error);

    // 序列化：2 空格缩进 + 结尾换行（和仓库里现有 JSON 的写法一致）
    std::string serialize() const;

    // 未保存判定：和上次保存/加载时的文本比较（比 dirty 标记更可靠：改回原样就不算脏）
    bool isDirty() const;
    void markSaved() { this->savedText = this->serialize(); this->hasSaved = true; }

    const std::string& getError() const { return this->lastError; }

  protected:
    std::string      filePath;
    designer::Json    document = designer::Json::object();
    std::string      savedText;
    bool             hasSaved = false;
    std::string      lastError;
};

} // namespace designer
