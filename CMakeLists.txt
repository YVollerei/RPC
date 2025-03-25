cmake_minimum_required(VERSION 3.0)
project(Krpc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)   # 强制使用指定的C++标准

# 设置头文件目录
# include_directories(${CMAKE_SOURCE_DIR}/src/include)

find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

# 设置全局链接库
set(LIBS
    protobuf
    pthread
    zookeeper_mt
    muduo_net
    muduo_base
)

# 添加子目录
add_subdirectory(src)
add_subdirectory(example)