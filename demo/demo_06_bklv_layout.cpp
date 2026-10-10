// Demo 06：用 bklv 移植层搭一个小布局。
//
// 展示的能力：
//   Box 的内边距、外边距、主轴分布、交叉轴对齐、gap、flex-grow
//   Label 的自动换行、单行省略号、字号与对齐
//
// 场景内容在 demo_06_scene.h，方便 demo_06_test.cpp 复用同一棵树。

#include "bklv/core/app.hpp"
#include "demo_06_scene.h"

int main()
{
    return RunDemo("Demo 06 - bklv layout", buildScene(), 1280, 800);
}
