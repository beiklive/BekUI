#pragma once

// 帧时间统计：把"卡不卡"变成数字。
//
// 这一层是被"界面永不卡顿"这个目标逼出来的：没有最坏帧与超时帧计数，"不卡"就只能靠感觉，
// 改完也不知道是变好了还是把问题挪到了别处。`App` 每帧记一个样本（整个循环迭代的时间，
// 含后端 present），`getSummary()` 给 p50 / p95 / p99 / 最坏帧。
//
// 纯逻辑、无窗口、无时钟依赖（时间由调用方传进来），所以能直接单测，见 tests/frame_stats_test.cpp。
// 环形缓冲只留最近 `kCapacity` 帧：问的是"现在表现如何"，不是"开机以来"。

#include <cstddef>

namespace bklv
{

struct FrameTimeSummary
{
    size_t count        = 0; // 参与统计的样本数（<= 容量）
    size_t droppedCount = 0; // 超过 16.67ms（60Hz 一帧）的帧数
    double averageMs    = 0.0;
    double p50Ms        = 0.0;
    double p95Ms        = 0.0;
    double p99Ms        = 0.0;
    double worstMs      = 0.0;
};

class FrameStats
{
  public:
    // 60Hz 一帧的时间：掉帧的判定线（帧时间超过它就可能少一次 vsync）
    static constexpr double kOneFrameMs = 1000.0 / 60.0;

    // 记一帧（传秒，和 core/time.hpp 一套单位）
    void record(double seconds);

    void reset();

    size_t getCount() const { return this->count; }
    static constexpr size_t getCapacity() { return kCapacity; }

    // 超过某个阈值的帧数（阈值单位毫秒）
    size_t getCountAbove(double thresholdMs) const;

    // 分位数 + 掉帧数（样本为空时各项为 0）
    FrameTimeSummary getSummary() const;

  private:
    static constexpr size_t kCapacity = 240; // 60fps 下约 4 秒
    double samplesMs[kCapacity] = {};
    size_t count = 0;  // 已填入的样本数（最多 kCapacity）
    size_t next  = 0;  // 下一个写入位置
};

} // namespace bklv
