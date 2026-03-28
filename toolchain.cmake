# 告知 CMake 目标系统和架构
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# 指定 C 编译器和 C++ 编译器
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# 目标 64-bit 并且启用本机优化 (-m64 -O3 -march=native)
set(CMAKE_C_FLAGS "-m64 -O3 -march=native" CACHE STRING "C Compiler Flags" FORCE)
set(CMAKE_CXX_FLAGS "-m64 -O3 -march=native" CACHE STRING "C++ Compiler Flags" FORCE)

# 由于是本机编译，移除交叉编译相关的 sysroot 和 FIND_ROOT_PATH 设置
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)