# 安装后消费示例

```bash
# 1. 安装（当前安装包要求关闭可选解码器）
cmake -S . -B /tmp/bklv_inst \
  -DBKLV_GRAPHICS=gl -DBKLV_BUILD_DEMO=OFF -DBKLV_BUILD_EXAMPLES=OFF \
  -DBKLV_WITH_WEBP=OFF -DBKLV_WITH_SVG=OFF \
  -DBKLV_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/tmp/bklv_prefix
cmake --build /tmp/bklv_inst -j8
cmake --install /tmp/bklv_inst

# 2. 消费
cmake -S examples/cmake_consumer -B /tmp/bklv_consumer_build \
  -DCMAKE_PREFIX_PATH=/tmp/bklv_prefix
cmake --build /tmp/bklv_consumer_build
/tmp/bklv_consumer_build/consumer
```

安装包当前是“源码内构建 + 静态库”形式，SDL3 静态库与最小 SDL3Config 一并安装；
可选解码器（WebP/SVG）暂未纳入导出。
