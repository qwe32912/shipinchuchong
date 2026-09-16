cmake_minimum_required(VERSION 3.14)
project(version_hijack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# 创建共享库 (DLL)
add_library(version SHARED version.cpp)

# 使用 CMake 的 FetchContent 自动从 GitHub 下载并集成 MinHook
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/tsudakageyu/minhook.git
    GIT_TAG master
)
FetchContent_MakeAvailable(minhook)

# 将 MinHook 链接到你的 DLL 中
target_link_libraries(version PRIVATE minhook)

# 确保目标输出文件名为 version.dll
set_target_properties(version PROPERTIES OUTPUT_NAME "version")
