#pragma once

// 宿主可替换的服务（M1）。
//
// 目前只覆盖两个最影响嵌入的点：
//   · ResourceProvider：把 resources/ 的路径解析交给宿主（自定义根、打包资源、内存资源）；
//   · AudioService：宿主接管音效（自己的混音器 / 平台音频 / 静音）。
//
// 默认都是 nullptr = 用库内现有实现。服务由宿主拥有，bklv 只借用；App 销毁前
// 宿主应 clearServices() 或保证服务活得更久。

#include <cstdint>
#include <string>
#include <vector>

#include "bklv/core/audio.hpp"

namespace bklv
{

class ResourceProvider
{
  public:
    virtual ~ResourceProvider() = default;

    // 逻辑路径（如 "resources/font/ui.ttf"）→ 实际路径/标识
    virtual std::string resolve(const std::string& relative) const = 0;

    // 读取资源；默认按 resolve() 的文件路径读。内存/打包资源可重写。
    virtual bool read(const std::string& relative, std::vector<unsigned char>& out,
        std::string& error) const;

    virtual bool hasResource(const std::string& relative) const;
};

class AudioService
{
  public:
    virtual ~AudioService() = default;

    virtual void play(Sound sound) = 0;
    virtual void playFile(const std::string& path) = 0;

    virtual void setEnabled(bool) { }
    virtual void setVolume(float) { }
};

struct Services
{
    AudioService* audio       = nullptr;
    ResourceProvider* resources = nullptr;
};

// 全局服务表（M1 仍是单实例；多实例在 Context 阶段迁移）
void setServices(const Services& services);
const Services& getServices();
void clearServices();

// 默认实现：不改变现有行为（资源走 resourcePath，音效走内置混音器）
class DefaultResourceProvider : public ResourceProvider
{
  public:
    std::string resolve(const std::string& relative) const override;
};

} // namespace bklv
