#pragma once

// 解码/上传的**背压调度**（纯数据 + 可注入回调，不碰 ImGui/后端，便于无窗口测试）。
//
// 背景：一次滚出 20 个新 cell 就是 20 次解码 + 20 次纹理上传 -> 主线程掉帧。
// 协议（docs/list-incremental-loading-design.md §11.4 小步3）：
//   · 同时在飞的解码数不超过 `getDecodeConcurrency()`（0 = 不限）
//   · 每帧最多把 `getMaxUploadsPerFrame()` 个"解码完成"的结果上传成纹理（0 = 不限）
//   · 队列按优先级（越小越优先，调用方给"离可见区中心距离"）出队
//   · 取消（视图销毁/换图/滚出视口）直接丢弃，不占上传配额
//
// 本文件只负责**调度**：真正的"启动解码"与"执行上传"由外部以回调形式注入。

#include <cstddef>
#include <functional>
#include <string>

namespace bklv
{

// 入队一个解码任务。start 会在允许时被调用（由 pumpDecodeQueue 驱动）。
// 同名 key 重复入队会被忽略（同一张图不重复解）。
void enqueueDecode(const std::string& key, int priority, std::function<void()> start);

// 每帧调用：按并发上限启动若干解码（按 priority 出队）。
void pumpDecodeQueue();

// 解码完成后由外部调用：把结果放入"待上传"队列（由 pumpUploads 按配额消费）。
void notifyDecodeFinished(const std::string& key, std::function<void()> upload);

// 每帧调用：按每帧配额执行上传。
void pumpUploads();

// 取消（滚出视口/换图/视图销毁）：从所有队列里移除该 key。
void cancelDecode(const std::string& key);

// 只读观察（测试/调试）
size_t getPendingDecodeCount();
size_t getInFlightDecodeCount();
size_t getPendingUploadCount();

// 测试用：清空
void clearDecodeQueueForTest();

} // namespace bklv
