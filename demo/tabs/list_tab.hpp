#pragma once

// 「列表」标签页：虚拟化长列表、虚拟化网格、横向滚动、以及"点条目打开新页面"。
//
// 三个 RecyclerFrame（同一个控件，参数不同）：
//   - 长列表：1 列、2000 条、行高 72，点条目 → 内容层下钻一层新页面
//   - 网格：3 列、400 格、格高 96（columns > 1 由 RecyclerFrame 自己排）
//   - 横向：HScrollingFrame 里横着摆 12 张卡（不是虚拟化，数量少）
//
// 复用只发生在"滚出视口的 cell"上：滚动时 cell 数量应稳定在视口行数 + 预取附近
// （DemoApp::getRecyclerStats() 把实测数字打在页面顶部）。

namespace bklv
{
class View;
}

namespace demo
{

class DemoApp;

bklv::View* buildListTab(DemoApp& app);

} // namespace demo
