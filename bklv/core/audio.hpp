#pragma once

// 音效反馈。
//
// 对应 borealis 的 core/audio.hpp（8 种系统音效）。差别：
// - borealis **只在 Switch 上出声**（用 Nintendo 的内置系统音效），桌面是什么都不做的。
//   我们改成**代码合成**：每种音效就是一段衰减正弦/方波，所以桌面和 Switch 都有反馈，
//   而且不用随包带任何音频素材（也就没有素材许可问题）。
// - 混音逻辑（`AudioMixer`）是纯计算、不碰设备，所以能无窗口测。
// - 设备那层：桌面用 SDL3 音频；Switch 的 audren 还没接（见 README/TODO，需要真机调）。

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace bklv
{

// 解码后的音效（单声道 48kHz；由 core/sound_bank 装载）
struct SoundClip
{
    std::vector<float> samples;
};
using SoundClipPtr = std::shared_ptr<const SoundClip>;

// 和 borealis 的 Sound 一一对应
enum class Sound
{
    NONE = 0,
    FOCUS_CHANGE,  // 焦点移动
    FOCUS_ERROR,   // 走到不能走的方向（borealis 高亮还会抖一下）
    CLICK,        // 确认/点击
    BACK,         // 返回
    FOCUS_SIDEBAR, // 焦点进侧栏
    CLICK_ERROR,   // 点了禁用控件
    HONK,         // 彩蛋
};

// 合成 + 混音（可无窗口测试）
class AudioMixer
{
  public:
    static constexpr int MAX_VOICES = 8;

    // 触发一个音效（代码合成）
    void trigger(Sound sound);
    // 触发一个采样音效（主题 sound.json / 外观覆盖）；空指针忽略
    void triggerClip(const SoundClipPtr& clip);

    // 把混好的单声道样本写进 out（frames 个，范围 -1..1）。
    // 没有活动音效时填 0 并返回 false（调用方可以把设备挂起省电）。
    bool fill(float* out, int frames);

    bool isSilent() const;

    void setEnabled(bool value);
    bool isEnabled() const { return this->enabled; }

    // 0~1
    void setVolume(float value);
    float getVolume() const { return this->volume; }

    // 测试用：当前还在发声的音效数量
    int getActiveVoiceCount() const;

  private:
    struct Voice
    {
        Sound sound   = Sound::NONE;
        SoundClipPtr clip;
        size_t cursor = 0;
        float phase   = 0.0f;
        float elapsed = 0.0f; // 秒
        bool active   = false;
    };

    // 每种音效的合成参数
    struct SoundSpec
    {
        float frequency = 440.0f;
        float duration  = 0.05f; // 秒
        float gain      = 0.25f;
        bool square     = false; // 方波（更"点击"味）/ 正弦
    };

    static SoundSpec specOf(Sound sound);

    Voice voices[MAX_VOICES];
    bool enabled = true;
    float volume = 0.6f;
    int sampleRate = 48000;
};

namespace audio
{

// 打开音频设备（桌面 SDL3；Switch 未实现，返回 false 并记一次日志）
bool init();
void shutdown();
bool isAvailable();

void setEnabled(bool value);
bool isEnabled();

void setVolume(float value);
float getVolume();

// 播一个音效（未初始化/禁用时什么都不做）：优先主题 sound.json，其次代码合成
void play(Sound sound);
// 播一个显式路径（外观里的 states.click.sound）
void playFile(const std::string& path);

} // namespace audio

} // namespace bklv
