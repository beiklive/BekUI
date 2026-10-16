#include "bklv/core/frame_stats.hpp"

#include <algorithm>
#include <vector>

namespace bklv
{

void FrameStats::record(double seconds)
{
    if (seconds < 0.0)
        seconds = 0.0;

    this->samplesMs[this->next] = seconds * 1000.0;
    this->next                  = (this->next + 1) % kCapacity;
    if (this->count < kCapacity)
        this->count++;
}

void FrameStats::reset()
{
    for (size_t i = 0; i < kCapacity; i++)
        this->samplesMs[i] = 0.0;
    this->count = 0;
    this->next  = 0;
}

size_t FrameStats::getCountAbove(double thresholdMs) const
{
    size_t above = 0;
    for (size_t i = 0; i < this->count; i++)
    {
        if (this->samplesMs[i] > thresholdMs)
            above++;
    }
    return above;
}

FrameTimeSummary FrameStats::getSummary() const
{
    FrameTimeSummary summary;
    if (this->count == 0)
        return summary;

    std::vector<double> sorted(this->samplesMs, this->samplesMs + this->count);
    std::sort(sorted.begin(), sorted.end());

    double total = 0.0;
    for (double value : sorted)
        total += value;

    // 百分位取"下侧最近秩序"（floor）：样本少时也不会越界，且 p50 就是中位数
    const auto percentile = [&sorted](double fraction) {
        size_t index = (size_t)(fraction * (double)(sorted.size() - 1));
        if (index >= sorted.size())
            index = sorted.size() - 1;
        return sorted[index];
    };

    summary.count        = this->count;
    summary.averageMs    = total / (double)this->count;
    summary.p50Ms        = percentile(0.50);
    summary.p95Ms        = percentile(0.95);
    summary.p99Ms        = percentile(0.99);
    summary.worstMs      = sorted.back();
    summary.droppedCount = this->getCountAbove(kOneFrameMs);
    return summary;
}

} // namespace bklv
