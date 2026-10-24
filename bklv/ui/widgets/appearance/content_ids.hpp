#pragma once

// 通用内容槽的**约定 id**（不是通用类的一部分）。
//
// `AppearanceView` 只知道"按 id 取槽"，不认得具体名字；这些常量是通用控件与外观 JSON 之间的
// 公共约定：外观里写 `"data_id": "text"`，控件就写 `setText(content::TEXT, …)`。
// 想加新槽（"badge" / "trailing" / "avatar"）直接用字符串 id 或在这里加常量，
// 不需要动 AppearanceView。
//
// 按钮家族不用这一套：它的内容位是 `button_slot` 里的 `m_icon` / `m_text` /
// `m_subtext` / `m_infotext`（见 `ui/widgets/controls/button.hpp`）。

namespace bklv::content
{

inline constexpr char TEXT[]  = "text";
inline constexpr char ICON[]  = "icon";
inline constexpr char INFO[]  = "info";
inline constexpr char IMAGE[] = "image";

} // namespace bklv::content
