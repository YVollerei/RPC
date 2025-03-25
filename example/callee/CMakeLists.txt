# 设置服务端源文件
set(SERVER_SRCS
    Kserver.cpp
)

# 获取protobuf生成的.cc
file(GLOB PROTO_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../*.pb.cc)

# 创建可执行文件server
add_executable(server ${SERVER_SRCS} ${PROTO_SRCS})

# 链接src文件夹中生成的krpc_core静态库，以及全局链接库
target_link_libraries(server krpc_core ${LIBS})

# 设置编译选项
target_compile_options(server PRIVATE -std=c++17 -Wall)

# 设置server可执行文件的输出目录
set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)