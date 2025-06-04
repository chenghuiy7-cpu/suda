cmake_minimum_required(VERSION 3.0)
project(grep)

# 设置包含路径
set(VITIS_HLS_INCLUDE_PATH "/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include")
set(PROJECT_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

# 定义ARM编译器路径
set(ARM_COMPILER_BASE_PATH "/opt/Xilinx_2020.2/Vitis/2020.2/gnu/aarch64/lin/aarch64-linux/bin")
set(ARM_C_COMPILER "${ARM_COMPILER_BASE_PATH}/aarch64-linux-gnu-gcc")
set(ARM_CXX_COMPILER "${ARM_COMPILER_BASE_PATH}/aarch64-linux-gnu-g++")

# 定义X86编译器路径
set(X86_COMPILER_BASE_PATH "/opt/Xilinx_2020.2/Vitis_HLS/2020.2/lnx64/tools/gcc/bin/")
set(X86_C_COMPILER "/usr/bin/gcc")
set(X86_CXX_COMPILER "/usr/bin/g++")

# 首先编译ARM版本（使用项目默认编译器）
set(CMAKE_C_COMPILER ${ARM_C_COMPILER})
set(CMAKE_CXX_COMPILER ${ARM_CXX_COMPILER})
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

add_definitions(-DEXPORT_SYMBOLS)

# 生成ARM动态链接库
add_library(grep SHARED
    sw_wrapper.cpp
    SimpleGrepApplication.cpp
)

# 设置ARM库的包含目录
target_include_directories(grep PRIVATE
    ${VITIS_HLS_INCLUDE_PATH}
    ${PROJECT_INCLUDE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 设置ARM库的编译选项
target_compile_options(grep PRIVATE 
   # 指定CPU和特性
    -mcpu=cortex-a53       # 指定目标为Cortex-A53
    -mtune=cortex-a53      # 特定针对A53调优

    # 指令集特性
    -ffast-math
    -fPIC 
    -flto
    -fpermissive
    -Wno-deprecated-declarations
    -Wno-attributes
    -Wno-strict-aliasing
    -g0
    -O3
)

# 设置ARM库的版本信息和链接选项
set_target_properties(grep PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    COMPILE_FLAGS "-fvisibility=hidden "
    LINK_FLAGS "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/sw_wrapper.map"
)


# 打印编译信息
message(STATUS "System name: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "ARM C Compiler: ${ARM_C_COMPILER}")
message(STATUS "ARM CXX Compiler: ${ARM_CXX_COMPILER}")
message(STATUS "X86 C Compiler: ${X86_C_COMPILER}")
message(STATUS "X86 CXX Compiler: ${X86_CXX_COMPILER}")
message(STATUS "Include directories:")
message(STATUS "  - Current source dir: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  - Vitis HLS include: ${VITIS_HLS_INCLUDE_PATH}")
message(STATUS "  - Project include: ${PROJECT_INCLUDE_PATH}")