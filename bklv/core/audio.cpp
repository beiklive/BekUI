#include "bklv/core/audio.hpp"

#include <cmath>
#include <mutex>

#include "bklv/core/logger.hpp"

#if !defined(BKLV_SWITCH)
#include <SDL3/SDL.h>
#endif

namespace bklv
{

namespace
{

constexpr float PI = 3.14159265f;

} // namespace

// ---------------------------------------------------------------- 合成与混音

AudioMixer::SoundSpec AudioMixer::specOf(Sound sound)
{
    switch (sound)
    {
        case Sound::FOCUS_CHANGE: // 短促、清亮
            return { 880.0f, 0.035f, 0.20f, false };
        case Sound::FOCUS_SIDEBAR: // 比焦点音低一点，区分开
            return { 620.0f, 0.045f, 0.20f, false };
        case Sound::CLICK: // 干脆的点击
            return { 1320.0f, 0.030f, 0.28f, true };
        case Sound::BACK: // 下行感：低一些 + 长一点
            return { 420.0f, 0.06f, 0.22f, false };
        case Sound::FOCUS_ERROR: // 低沉 + 抖（用低频方波模拟"走到头了"）
            return { 180.0f, 0.18f, 0.24f, true };
        case Sound::CLICK_ERROR: // 更闷的一声
            return { 120.0f, 0.12f, 0.22f, true };
        case Sound::HONK: // 彩蛋
            return { 240.0f, 0.30f, 0.30f, true };
        case Sound::NONE:
            break;
    }
    return { 0.0f, 0.0f, 0.0f, false };
}

void AudioMixer::trigger(Sound sound)
{
    if (!this->enabled || sound == Sound::NONE)
        return;

    // 优先用空闲声道；都占着就抢最旧的那个
    Voice* target = nullptr;
    for (Voice& voice : this->voices)
    {
        if (!voice.active)
        {
            target = &voice;
            break;
        }
    }

    if (target == nullptr)
    {
        float oldest = -1.0f;
        for (Voice& voice : this->voices)
        {
            if (voice.elapsed > oldest)
            {
                oldest = voice.elapsed;
                target = &voice;
            }
        }
    }

    if (target == nullptr)
        return;

    const SoundSpec spec = specOf(sound);

    target->sound   = sound;
    target->phase   = 0.0f;
    target->elapsed = 0.0f;
    target->active  = spec.duration > 0.0f;
    (void)spec;
}

bool AudioMixer::fill(float* out, int frames)
{
    if (out == nullptr || frames <= 0)
        return false;

    for (int i = 0; i < frames; i++)
        out[i] = 0.0f;

    bool produced = false;

    for (Voice& voice : this->voices)
    {
        if (!voice.active)
            continue;

        const SoundSpec spec = specOf(voice.sound);
        produced             = true;

        for (int i = 0; i < frames; i++)
        {
            if (voice.elapsed >= spec.duration)
            {
                voice.active = false;
                break;
            }

            // 方波听起来更"电子"，正弦更柔和
            float sample = 0.0f;
            if (spec.square)
                sample = std::sin(voice.phase) >= 0.0f ? 1.0f : -1.0f;
            else
                sample = std::sin(voice.phase);

            // 线性衰减包络（末尾 30% 再快一点，避免"啪"的截断声）
            const float t        = voice.elapsed / spec.duration;
            const float envelope = 1.0f - t * t;

            out[i] += sample * envelope * spec.gain * this->volume;

            voice.phase += 2.0f * PI * spec.frequency / (float)this->sampleRate;
            if (voice.phase > 2.0f * PI)
                voice.phase -= 2.0f * PI;

            voice.elapsed += 1.0f / (float)this->sampleRate;
        }
    }

    // 软限幅：多个音效叠在一起也不破音
    for (int i = 0; i < frames; i++)
    {
        if (out[i] > 1.0f)
            out[i] = 1.0f;
        else if (out[i] < -1.0f)
            out[i] = -1.0f;
    }

    return produced;
}

bool AudioMixer::isSilent() const
{
    for (const Voice& voice : this->voices)
        if (voice.active)
            return false;
    return true;
}

void AudioMixer::setEnabled(bool value)
{
    this->enabled = value;
    if (!value)
        for (Voice& voice : this->voices)
            voice.active = false;
}

void AudioMixer::setVolume(float value)
{
    this->volume = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

int AudioMixer::getActiveVoiceCount() const
{
    int count = 0;
    for (const Voice& voice : this->voices)
        if (voice.active)
            count++;
    return count;
}

// ---------------------------------------------------------------- 设备那层

namespace audio
{

namespace
{

AudioMixer g_mixer;
bool g_available = false;
std::mutex g_mutex;

#if !defined(BKLV_SWITCH)
SDL_AudioStream* g_stream = nullptr;

void audioCallback(void*, SDL_AudioStream* stream, int additionalAmount, int)
{
    if (stream == nullptr || additionalAmount <= 0)
        return;

    // SDL 推给我们的是**额外需要的字节数**（S16 单声道）
    static float scratch[4096];
    const int totalSamples = additionalAmount / (int)sizeof(Sint16);

    int done = 0;
    while (done < totalSamples)
    {
        const int chunk = (totalSamples - done) > (int)(sizeof(scratch) / sizeof(float))
            ? (int)(sizeof(scratch) / sizeof(float))
            : (totalSamples - done);

        std::lock_guard<std::mutex> lock(g_mutex);
        g_mixer.fill(scratch, chunk);

        Sint16 converted[4096];
        for (int i = 0; i < chunk; i++)
            converted[i] = (Sint16)(scratch[i] * 32767.0f);

        SDL_PutAudioStreamData(stream, converted, chunk * (int)sizeof(Sint16));
        done += chunk;
    }
}
#endif

} // namespace

bool init()
{
    if (g_available)
        return true;

#if defined(BKLV_SWITCH)
    // Switch 的 audren 需要真机调（音频缓冲/服务初始化），暂时不接。
    // 桌面已经可用；这里只记一次日志，不阻塞启动。
    static bool warned = false;
    if (!warned)
    {
        warned = true;
        BKLV_LOG_WARNING("Switch 音频还没接（audren 需要真机验证）；音效在 Switch 上暂时静音");
    }
    return false;
#else
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        BKLV_LOG_WARNING("SDL 音频子系统初始化失败：%s", SDL_GetError());
        return false;
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.format   = SDL_AUDIO_S16;
    spec.channels = 1;
    spec.freq     = 48000;

    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioCallback,
        nullptr);
    if (g_stream == nullptr)
    {
        BKLV_LOG_WARNING("打开音频设备失败：%s", SDL_GetError());
        return false;
    }

    SDL_ResumeAudioStreamDevice(g_stream);
    g_available = true;
    BKLV_LOG_INFO("音频已就绪（音效是代码合成的，不依赖素材）");
    return true;
#endif
}

void shutdown()
{
#if !defined(BKLV_SWITCH)
    if (g_stream != nullptr)
    {
        SDL_DestroyAudioStream(g_stream);
        g_stream = nullptr;
    }
#endif
    g_available = false;
}

bool isAvailable()
{
    return g_available;
}

void setEnabled(bool value)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_mixer.setEnabled(value);
}

bool isEnabled()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_mixer.isEnabled();
}

void setVolume(float value)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_mixer.setVolume(value);
}

float getVolume()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_mixer.getVolume();
}

void play(Sound sound)
{
    if (!g_available || sound == Sound::NONE)
        return;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_mixer.trigger(sound);
}

} // namespace audio

} // namespace bklv
