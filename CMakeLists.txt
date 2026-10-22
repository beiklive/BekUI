# bklv —— 仓库构建入口
#
# 这里只做三件事：判定平台、定输出目录、编出 demo 程序。
# **库本身的构建在 bklv/CMakeLists.txt**，那个目录是自包含的，可以整目录拷走。
#
# 后端是编译期决定的（见 AGENTS.md），所以「同一个 demo 编 GL 和 Vulkan 两版」
# 靠两个构建目录实现：
#
#   cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl    # -> dist/
#   cmake -S . -B build/vk -DBKLV_GRAPHICS=vk && cmake --build build/vk    # -> dist/vk/
#
# 用法见 README.md。

# 先只要求 3.16（老版本的 CMake 不会在这里直接硬报错），然后给一条**看得懂**的版本提示：
# 真正需要 3.24 的是 SDL3 子模块 —— 它用 $<LINK_LIBRARY:FRAMEWORK,...>（CMake 3.24 起才有）
# 链接 macOS 的系统框架；tweeny 子模块要 3.23。写死 3.24 的话，用户只会看到
# "CMake 3.24 or higher is required"，看不出是谁要求的、该怎么办。
cmake_minimum_required(VERSION 3.16)

if(CMAKE_VERSION VERSION_LESS 3.24)
    message(FATAL_ERROR
        "本项目需要 CMake ≥ 3.24，当前是 ${CMAKE_VERSION}。\n"
        "  · SDL3 子模块用 $<LINK_LIBRARY:FRAMEWORK,...> 链接 macOS 系统框架（3.24 起支持）\n"
        "  · tweeny 子模块要求 ≥ 3.23\n"
        "升级办法：\n"
        "  · macOS: brew upgrade cmake\n"
        "  · Ubuntu/Debian: 用 Kitware 的 apt 源装新版（发行版自带通常偏旧）\n"
        "  · Windows/MSYS2: pacman -S mingw-w64-x86_64-cmake\n"
        "  · 或者用 CMake 官方安装包 / pip install cmake")
endif()

# 先不带语言调用一次 project()，只为拿到 CMAKE_SYSTEM_NAME。
# 在 MSYS2 的「MSYS」环境里，CMake 判定的是 CMAKE_SYSTEM_NAME=MSYS 而 WIN32 为空；
# 真正的编译器探测（会失败）发生在带 LANGUAGES 的 project() 里，所以必须先探明平台，
# 再决定要不要自己找工具链。
project(bklv LANGUAGES NONE)

# ---------------------------------------------------------------------------
# MSYS2：编译器兜底
# ---------------------------------------------------------------------------
# MSYS2 里的 cmake 是跑在 MSYS 运行时上的 /usr/bin/cmake.exe。如果 PATH 里只有
# usr/bin 而没有 mingw64/bin，它的自动探测找不到 g++，报
# "No CMAKE_C_COMPILER could be found"——于是「在 MSYS2 shell 里直接 cmake」会失败。
# MINGW64 环境自带 MINGW_PREFIX，这里直接用它把编译器指出来，省掉手动 export PATH。
if(CMAKE_SYSTEM_NAME STREQUAL "MSYS" OR CMAKE_SYSTEM_NAME STREQUAL "MINGW")
    if(DEFINED ENV{MINGW_PREFIX} AND NOT "$ENV{MINGW_PREFIX}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{MINGW_PREFIX}" BKLV_MINGW)
        if(EXISTS "${BKLV_MINGW}/bin/gcc.exe" AND EXISTS "${BKLV_MINGW}/bin/g++.exe")
            # 必须在带 LANGUAGES 的 project() 之前设好，并且写进 cache，
            # 否则会被那一步的系统探测丢掉。
            set(CMAKE_C_COMPILER   "${BKLV_MINGW}/bin/gcc.exe"
                CACHE FILEPATH "C 编译器" FORCE)
            set(CMAKE_CXX_COMPILER "${BKLV_MINGW}/bin/g++.exe"
                CACHE FILEPATH "C++ 编译器" FORCE)
            message(STATUS "MSYS2 工具链：${BKLV_MINGW}")
        endif()
    elseif(NOT DEFINED CMAKE_C_COMPILER)
        message(STATUS
            "在 MSYS2 里没有自动找到 MinGW 工具链。请用「MSYS2 MINGW64」快捷方式"
            "打开终端，或传 -DCMAKE_CXX_COMPILER=<路径>/g++.exe")
    endif()
endif()

# 真正开始探测语言。
project(bklv LANGUAGES C CXX)

# 默认 Release：MinGW 下不指定构建类型会得到 -O0，和预期的 -O2 差太多。
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "构建类型" FORCE)
endif()

option(BKLV_WINDOWS_GUI "Windows 上不弹控制台窗口（会看不到 BKLV_LOG_* 输出）" OFF)
set(BKLV_GRAPHICS "gl" CACHE STRING "图形后端：gl 或 vk")
set_property(CACHE BKLV_GRAPHICS PROPERTY STRINGS gl vk)

# 只编指定的目标，留空=全部。按完整名字匹配，可逗号分隔多个。
# 目前只有一个目标 demo，保留这个开关是为了以后加示例程序时不用改构建。
set(BKLV_TARGETS "" CACHE STRING "只构建列出的目标（完整名字，逗号分隔；留空=全部）")

# ---------------------------------------------------------------------------
# 平台判定
# ---------------------------------------------------------------------------

# 注意不能只看 WIN32：在 MSYS2 的 MSYS 终端里 CMAKE_SYSTEM_NAME 是 MSYS、WIN32 为空，
# 那样 BKLV_HOST 会掉进 linux 分支，Windows 的链接参数与 stb_image 分支就被静默跳过了。
# 生成的仍然是原生 Windows 程序，所以按 Windows 处理。
# bklv/CMakeLists.txt 在没有这个变量时会自己判定，这里设好传下去。
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "MSYS" OR CMAKE_SYSTEM_NAME STREQUAL "MINGW")
    set(BKLV_HOST "windows")
elseif(APPLE)
    set(BKLV_HOST "macos")
else()
    set(BKLV_HOST "linux")
endif()

# 输出目录：GL 走 dist/，Vulkan 走 dist/vk/。
if(BKLV_GRAPHICS STREQUAL "vk")
    set(BKLV_OUT_DIR "${CMAKE_SOURCE_DIR}/dist/vk")
elseif(BKLV_GRAPHICS STREQUAL "gl")
    set(BKLV_OUT_DIR "${CMAKE_SOURCE_DIR}/dist")
else()
    message(FATAL_ERROR "BKLV_GRAPHICS 只能是 gl 或 vk（当前：${BKLV_GRAPHICS}）")
endif()

# ---------------------------------------------------------------------------
# 库
# ---------------------------------------------------------------------------

add_subdirectory(bklv)

# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# 资源复制
# ---------------------------------------------------------------------------
#
# 资源按"可执行文件目录"解析（见 bklv/core/resource.hpp），所以每个可执行文件旁边
# 都要有一份 resources/：从哪个目录启动都能找到；测试写出来的 fixture 也落在
# dist/ 里，不会污染仓库（以前只有 demo 复制资源，测试就退化成按工作目录解析，
# 把 fixture 写进了仓库树，还会被打进 romfs）。
# 实现注意：**每个输出目录只复制一次**。
# 以前是给 21 个目标各挂一条 POST_BUILD copy_directory，全部往 dist/resources 写，
# -j 并行时互相踩 → 间歇性失败（`make: *** [all] Error 2`），而且失败后**二进制还是旧的**，
# 于是"改了代码没生效"这种假象会骗过验证。现在改成按输出目录建一个独立目标，
# 其余目标依赖它，复制只发生一次。
function(bklv_copy_resources target_name)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/resources")
        return()
    endif()

    # 所有目标共用 BKLV_OUT_DIR（见文件开头），按它建只复制一次的目标
    set(out_dir "${BKLV_OUT_DIR}")
    if(NOT out_dir)
        set(out_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()
    if(NOT out_dir)
        message(FATAL_ERROR "bklv_copy_resources: 输出目录未知（BKLV_OUT_DIR 为空）")
    endif()

    string(MAKE_C_IDENTIFIER "${out_dir}" dir_id)
    set(resource_target "bklv_copy_resources_${dir_id}")

    if(NOT TARGET ${resource_target})
        # 目标目录必须和源码目录分开：这条规则会**先删后拷**。
        if(out_dir STREQUAL "${CMAKE_SOURCE_DIR}"
            OR "${out_dir}/resources" STREQUAL "${CMAKE_SOURCE_DIR}/resources")
            message(FATAL_ERROR "bklv_copy_resources: 输出目录不能是仓库源码目录（${out_dir}）")
        endif()

        # 先删后拷，不是只 copy_directory：copy_directory **只加不删**，主题目录改布局之后
        # 旧布局的文件会一直留在 dist/resources 里（实测留了 45 个：根级 colors.json /
        # metrics.json 与整个 widgets/ 目录）。这些残留会骗过"磁盘 vs theme.json 的 files[]
        # 清单"这类校验（theme_test 第 9 节被它们报了 138 个假失败），也让 dist 里的资源
        # 和仓库不一致（2.4M vs 1.8M）。删的只是构建产物目录，仓库 resources/ 不动。
        add_custom_target(${resource_target}
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${out_dir}/resources"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/resources"
                "${out_dir}/resources"
            COMMENT "同步 resources/ 到 ${out_dir}（先删后拷，清掉已删除的文件）"
            VERBATIM
        )
    endif()

    add_dependencies(${target_name} ${resource_target})
endfunction()

# demo：页面 / tab / 列表项这一层应用代码 + 主程序
# ---------------------------------------------------------------------------
#
# 这一层属于 demo，不属于库：它用 bklv 的控件拼出「能力总览」这个具体界面。
# 显式列源文件，不用 GLOB：加删文件比 glob 的便利更重要。

set(BKLV_APP_SOURCES
    app/demo_app.cpp
    app/card_button.cpp
    app/pages/page_icons.cpp
    app/pages/page_tab_frame.cpp
    app/pages/page_button.cpp
    app/pages/page_layout.cpp
    app/pages/page_json_theme.cpp
    app/pages/page_cells.cpp
    app/pages/showcase.cpp
)

set(BKLV_APP_ABS "")
foreach(src IN LISTS BKLV_APP_SOURCES)
    list(APPEND BKLV_APP_ABS "${CMAKE_SOURCE_DIR}/demo/${src}")
endforeach()


add_library(demo_app OBJECT ${BKLV_APP_ABS})
target_link_libraries(demo_app PUBLIC bklv::bklv)

# demo 自己的源码写的是 #include "app/page_host.hpp"，所以要把 demo/ 加进包含路径。
target_include_directories(demo_app PUBLIC "${CMAKE_SOURCE_DIR}/demo")

# ---------------------------------------------------------------------------
# 设计器（子工程）：代码、状态、预设都在 designer/ 下，链接 bklv，资源走 resources/ 原路径
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/designer/CMakeLists.txt")
    add_subdirectory(designer)
endif()

# ---------------------------------------------------------------------------
# 目标程序
# ---------------------------------------------------------------------------

set(BKLV_TARGET_LIST
    "demo|demo/main.cpp|app"
)

# 清单漏项检查（同 bklv/CMakeLists.txt）：demo 下的 .cpp 必须都出现在
# BKLV_APP_SOURCES（应用代码）或 BKLV_TARGET_LIST（各目标的入口 main.cpp）里
set(BKLV_APP_ALLOWED ${BKLV_APP_ABS})
foreach(entry IN LISTS BKLV_TARGET_LIST)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 1 target_src)
    list(APPEND BKLV_APP_ALLOWED "${CMAKE_SOURCE_DIR}/${target_src}")
endforeach()

file(GLOB_RECURSE _bklv_demo_found CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/demo/*.cpp")
foreach(_file ${_bklv_demo_found})
    if(NOT _file IN_LIST BKLV_APP_ALLOWED)
        file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_file}")
        message(FATAL_ERROR
            "CMakeLists.txt 既没把 ${_rel} 放进 BKLV_APP_SOURCES，也没放进 BKLV_TARGET_LIST")
    endif()
endforeach()

set(BKLV_TARGET_NAMES "")
set(BKLV_MATCHED 0)

foreach(entry IN LISTS BKLV_TARGET_LIST)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 target_name)
    list(GET parts 1 target_src)
    list(GET parts 2 target_kind)

    if(NOT BKLV_TARGETS STREQUAL "")
        string(REPLACE "," ";" BKLV_TARGETS_LIST "${BKLV_TARGETS}")
        if(NOT target_name IN_LIST BKLV_TARGETS_LIST)
            continue()
        endif()
    endif()
    math(EXPR BKLV_MATCHED "${BKLV_MATCHED} + 1")

    add_executable(${target_name} "${CMAKE_SOURCE_DIR}/${target_src}")

    # SDL3 / OpenGL / Vulkan / ImGui 都由 bklv::bklv 带出来，不需要在这里重复列。
    target_link_libraries(${target_name} PRIVATE bklv::bklv)

    if(target_kind STREQUAL "app")
        target_link_libraries(${target_name} PRIVATE demo_app)
    endif()

    # Windows 默认保留控制台，BKLV_LOG_* 才看得见（与 macOS / Linux 行为一致）。
    if(BKLV_HOST STREQUAL "windows" AND BKLV_WINDOWS_GUI)
        target_link_options(${target_name} PRIVATE -mwindows)
    endif()

    bklv_copy_resources(${target_name})

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${BKLV_OUT_DIR}"
    )

    list(APPEND BKLV_TARGET_NAMES ${target_name})
endforeach()

if(NOT BKLV_TARGETS STREQUAL "" AND BKLV_MATCHED EQUAL 0)
    message(FATAL_ERROR
        "BKLV_TARGETS=${BKLV_TARGETS} 没有匹配到任何目标。可用目标：\n  demo")
endif()

# ---------------------------------------------------------------------------
# 无窗口测试（tests/）
# ---------------------------------------------------------------------------
#
# 只断言数据层面的行为（装载、查找、回退、格式化、缓存、JSON 解析错误），
# 不开窗口：失败时定位成本低得多。图形相关仍靠 demo 手测。
#
# 产物与 demo 同目录（dist/），因为资源路径是按可执行文件目录解析的
# （见 bklv/core/resource.hpp），测试要能读到 resources/。
option(BKLV_BUILD_TESTS "编译 tests/ 下的无窗口测试" OFF)

if(BKLV_BUILD_TESTS)
    file(GLOB BKLV_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*_test.cpp")

    foreach(test_src ${BKLV_TEST_SOURCES})
        get_filename_component(test_name "${test_src}" NAME_WE)

        add_executable(${test_name} "${test_src}")
        target_link_libraries(${test_name} PRIVATE bklv::bklv)

        # 有一个测试要验"第三方扩展层"这条路径，它**直接编 demo 里那份样例源码**：
        # 在测试里复制一份会慢慢漂移（stripe 的测试桩与样例就漂过），
        # 所以这里让测试编真源码，而不是再写一份仿制品。
        if(${test_name} STREQUAL "quad_layer_test")
            target_sources(${test_name} PRIVATE "${CMAKE_SOURCE_DIR}/tests/support/quad_layer.cpp")
            target_include_directories(${test_name} PRIVATE "${CMAKE_SOURCE_DIR}/tests")
        endif()
        if(TARGET designer_core)
            target_link_libraries(${test_name} PRIVATE designer_core)
        endif()
        bklv_copy_resources(${test_name})
        set_target_properties(${test_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${BKLV_OUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BKLV_OUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BKLV_OUT_DIR}"
        )

        # ctest 里跑；也能直接执行 dist/<name>
        add_test(NAME ${test_name} COMMAND ${test_name})
        list(APPEND BKLV_TEST_NAMES ${test_name})
    endforeach()

    enable_testing()
endif()

# ---------------------------------------------------------------------------
# 状态输出
# ---------------------------------------------------------------------------

list(LENGTH BKLV_TARGET_NAMES BKLV_TARGET_COUNT)

message(STATUS "bklv 仓库")
message(STATUS "  平台     : ${BKLV_HOST}")
message(STATUS "  输出目录 : ${BKLV_OUT_DIR}")
message(STATUS "  目标     : ${BKLV_TARGET_COUNT} 个 —— ${BKLV_TARGET_NAMES}")
if(BKLV_BUILD_TESTS)
    message(STATUS "  测试     : ${BKLV_TEST_NAMES}")
endif()
