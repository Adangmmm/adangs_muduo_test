# 获取当前目录下的所有源文件
file(GLOB TESTDEMO_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/.*cc)

# 创建可执行文件
add_executable(chatserver ${TESTDEMO_SRCS})

# 链接必要的库，src/下CMakeList里的adangs_muduo_lib，还有全局链接库
target_link_libraries(chatserver adangs_muduo ${LIBS})

# 设置编译选项
target_compile_options(chatserver PRIVATE -std=c++11 -Wall)

#设置 chatserver 可执行文件输出目录
set_target_properties(chatserver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


