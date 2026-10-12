// 无窗口测试：音效合成与混音（不碰音频设备）。

#include <cmath>
#include <cstdio>

#include "bklv/core/audio.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 渲染 n 帧，返回峰值
float renderPeak(AudioMixer& mixer, int frames, float* out)
{
    mixer.fill(out, frames);
    float peak = 0.0f;
    for (int i = 0; i < frames; i++)
        peak = std::max(peak, std::fabs(out[i]));
    return peak;
}

} // namespace

int main()
{
    constexpr int RATE = 48000;
    float buffer[RATE / 4];

    std::printf("== 1. 触发后有声音、到点自动停 ==\n");
    {
        AudioMixer mixer;
        check(mixer.isSilent(), "初始是静音");
        checkFloat(renderPeak(mixer, 128, buffer), 0.0f, "没触发时输出全 0");

        mixer.trigger(Sound::Click);
        check(!mixer.isSilent(), "触发后有活动声道");
        checkEqualInt(mixer.getActiveVoiceCount(), 1, "一个声道在响");
        check(renderPeak(mixer, 256, buffer) > 0.05f, "渲染出非零样本");

        // 一直渲染到超过时长（Click 是 30ms，这里按块渲染 1 秒的量）
        const int chunk = (int)(sizeof(buffer) / sizeof(float));
        for (int played = 0; played < RATE && !mixer.isSilent(); played += chunk)
            mixer.fill(buffer, chunk);
        check(mixer.isSilent(), "超过时长后自动静音");
        checkEqualInt(mixer.getActiveVoiceCount(), 0, "声道释放");
    }

    std::printf("\n== 2. 音量与开关 ==\n");
    {
        AudioMixer loud;
        loud.setVolume(1.0f);
        loud.trigger(Sound::Click);
        const float loudPeak = renderPeak(loud, 256, buffer);

        AudioMixer quiet;
        quiet.setVolume(0.25f);
        quiet.trigger(Sound::Click);
        const float quietPeak = renderPeak(quiet, 256, buffer);

        check(quietPeak < loudPeak, "音量小则峰值小");
        check(quietPeak > 0.0f, "音量小也还有声音");

        AudioMixer muted;
        muted.setEnabled(false);
        muted.trigger(Sound::Click);
        check(muted.isSilent(), "禁用后触发不了");
        checkFloat(renderPeak(muted, 256, buffer), 0.0f, "禁用后输出全 0");

        // 响到一半再禁用：立刻静音
        AudioMixer half;
        half.trigger(Sound::Honk);
        half.fill(buffer, 128);
        half.setEnabled(false);
        check(half.isSilent(), "播放中禁用会立刻停");
    }

    std::printf("\n== 3. 多个音效叠加不破音 ==\n");
    {
        AudioMixer mixer;
        mixer.setVolume(1.0f);
        for (int i = 0; i < 6; i++)
            mixer.trigger(Sound::Click);
        check(mixer.getActiveVoiceCount() > 1, "多个声道同时在响");

        mixer.fill(buffer, 512);
        float peak = 0.0f;
        for (int i = 0; i < 512; i++)
            peak = std::max(peak, std::fabs(buffer[i]));
        check(peak <= 1.0f, "软限幅：叠加后不超过 1.0");
    }

    std::printf("\n== 4. 边界情况 ==\n");
    {
        AudioMixer mixer;
        mixer.trigger(Sound::None);
        check(mixer.isSilent(), "None 不发声");
        checkFloat(renderPeak(mixer, 64, buffer), 0.0f, "None 输出 0");

        // 声道被占满后再触发：抢最旧的，不崩
        for (int i = 0; i < AudioMixer::MAX_VOICES + 3; i++)
            mixer.trigger(Sound::Honk);
        check(mixer.getActiveVoiceCount() <= AudioMixer::MAX_VOICES, "声道数不超过上限");

        checkFloat(mixer.fill(nullptr, 16) ? 1.0f : 0.0f, 0.0f, "空指针安全");
        checkFloat(mixer.fill(buffer, 0) ? 1.0f : 0.0f, 0.0f, "0 帧安全");
    }

    std::printf("\n== 5. 设备层（无窗口下不可用，但不能崩）==\n");
    {
        check(!audio::isAvailable(), "测试环境没有音频设备");
        audio::play(Sound::Click); // 未初始化时什么都不做
        audio::setEnabled(false);
        check(!audio::isEnabled(), "开关可读写");
        audio::setVolume(0.3f);
        checkFloat(audio::getVolume(), 0.3f, "音量可读写");
        audio::setEnabled(true);
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
