#include "bklv/core/frame_budget.hpp"

#include "bklv/core/globals.hpp"
#include "bklv/core/time.hpp"

namespace bklv::budget
{

namespace
{

bool g_frameActive = false;
double g_budgetMs  = 0.0;   // <= 0 = 不限制
double g_deadline  = 0.0;   // 单调时钟秒
size_t g_deferred  = 0;

// 无活动帧时给一个"用不完"的剩余量，调用方不用为"没设预算"写特判
constexpr double kUnlimitedMs = 1.0e9;

} // namespace

void beginFrame(double budgetMs)
{
    g_frameActive = true;
    g_budgetMs    = budgetMs;
    g_deferred    = 0;

    if (budgetMs > 0.0)
    {
        g_deadline = getSecondsNow() + budgetMs / 1000.0;
    }
    else
    {
        g_deadline = 0.0;
    }

    // 帧预算是进程级状态：登记一个重置钩子（约定见 core/globals.hpp）
    globals::getGlobalStore().addResetHook("frameBudget", &resetFrameBudget);
}

void endFrame()
{
    g_frameActive = false;
    g_budgetMs    = 0.0;
    g_deadline    = 0.0;
}

bool isFrameActive()
{
    return g_frameActive;
}

bool isExhausted()
{
    if (!g_frameActive || g_budgetMs <= 0.0)
        return false;
    return getSecondsNow() >= g_deadline;
}

double getRemainingMs()
{
    if (!g_frameActive || g_budgetMs <= 0.0)
        return kUnlimitedMs;
    const double remaining = (g_deadline - getSecondsNow()) * 1000.0;
    return remaining > 0.0 ? remaining : 0.0;
}

double getBudgetMs()
{
    return g_budgetMs;
}

void noteDeferred()
{
    g_deferred++;
}

size_t getDeferredCount()
{
    return g_deferred;
}

void resetFrameBudget()
{
    g_frameActive = false;
    g_budgetMs    = 0.0;
    g_deadline    = 0.0;
    g_deferred    = 0;
}

} // namespace bklv::budget
