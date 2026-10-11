#pragma once

// 资源路径解析。
//
// 各平台资源的存放方式不同，但代码里希望写同一句相对路径：
//   Switch  资源由 elf2nro --romfsdir 打进 nro，运行时 romfsInit() 挂成
//           "romfs:/"，所以解析成 "romfs:/resources/xxx.png"
//   桌面    构建会把 resources/ 复制到可执行文件旁边，解析成
//           "<可执行文件目录>/resources/xxx.png"
//
// 这样"从哪个目录启动"都不影响资源查找——用工作目录相对路径的话，
// 换个目录启动就找不到图了。

#include <string>

namespace bklv
{

// 准备好资源文件系统。Switch 上挂 romfs，桌面是空实现。
// App 构造时会调用；自己写入口的话要在读资源之前调一次。
void initResources();

// 把相对资源路径解析成平台上能直接打开的路径。
// 已经是绝对路径 / 设备路径（romfs:/、sdmc:/、/、C:\、带 :// 的 URL）时原样返回。
std::string resourcePath(const std::string& relative);

} // namespace bklv
