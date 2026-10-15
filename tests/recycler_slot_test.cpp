// 无窗口测试：slots —— loading/empty 由数据源选，JSON 侧只声明槽位长什么样。

#include <cstdio>
#include <string>

#include "bklv/core/json_view.hpp"
#include "bklv/views/data_source.hpp"
#include "bklv/widgets/recycler_view.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

class SlotSource : public RecyclerDataSource
{
  public:
    size_t getItemCount() override { return 0; }
    float getItemHeight(size_t) override { return 72.0f; }
    void setState(RecyclerSlotState value) { this->state = value; }
    RecyclerSlotState getSlotState() override { return this->state; }

  private:
    RecyclerSlotState state = RecyclerSlotState::CONTENT;
};

} // namespace

int main()
{
    std::printf("== 1. 默认 CONTENT：两个槽位都不显示 ==\n");
    {
        SlotSource source;
        RecyclerView list;
        list.setSlotLoading("children_ref_demo");
        list.setSlotEmpty("children_ref_demo");
        list.setDataSource(&source);
        list.update();

        check(list.getSlotState() == RecyclerSlotState::CONTENT, "状态是 CONTENT");
        check(!list.isSlotVisible(RecyclerSlotState::LOADING), "loading 槽位不显示");
        check(!list.isSlotVisible(RecyclerSlotState::EMPTY), "empty 槽位不显示");
    }

    std::printf("\n== 2. 数据源说 LOADING / EMPTY -> 对应槽位显示 ==\n");
    {
        SlotSource source;
        RecyclerView list;
        list.setSlotLoading("children_ref_demo");
        list.setSlotEmpty("children_ref_demo");
        list.setDataSource(&source);
        list.update();

        source.setState(RecyclerSlotState::LOADING);
        list.update();
        check(list.getSlotState() == RecyclerSlotState::LOADING, "状态变成 LOADING");
        check(list.isSlotVisible(RecyclerSlotState::LOADING), "loading 槽位显示");
        check(!list.isSlotVisible(RecyclerSlotState::EMPTY), "empty 槽位不显示");

        source.setState(RecyclerSlotState::EMPTY);
        list.update();
        check(list.getSlotState() == RecyclerSlotState::EMPTY, "状态变成 EMPTY");
        check(list.isSlotVisible(RecyclerSlotState::EMPTY), "empty 槽位显示");
        check(!list.isSlotVisible(RecyclerSlotState::LOADING), "loading 槽位收起");

        source.setState(RecyclerSlotState::CONTENT);
        list.update();
        check(!list.isSlotVisible(RecyclerSlotState::EMPTY), "回到 CONTENT 后槽位收起");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
