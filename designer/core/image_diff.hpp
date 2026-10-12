#pragma once

// 截图基线对比：改完界面之后"和上次比变了没、变在哪"。
// 无窗口可测：输入是两张 PNG 的路径，输出是差异统计 + 一张标红差异图。

#include <string>

namespace designer
{

struct ImageDiff
{
    bool sameSize      = false;
    int width          = 0;
    int height         = 0;
    int otherWidth     = 0;
    int otherHeight    = 0;
    float meanDiff     = 0.0f; // 0..255，全图平均通道差
    int maxDiff        = 0;    // 单通道最大差
    float changedRatio = 0.0f; // 有明显差异的像素占比（阈值 8/255）
};

// 逐像素比。尺寸不同只报尺寸（不做缩放对齐，故意简单）
ImageDiff compareImages(const std::string& pathA, const std::string& pathB, std::string& error);

// 把差异画出来：一样的像素压暗，不同的像素标红（a 有 b 没有的标红，反之标蓝）
bool writeDiffImage(const std::string& outputPath, const std::string& pathA,
    const std::string& pathB, std::string& error);

} // namespace designer
