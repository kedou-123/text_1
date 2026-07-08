cmake_minimum_required(VERSION 3.15)
project(TestSolution)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(my_core STATIC
    ./str/ranker.c
    ./str/query_processor.c
    ./str/indexer.c
    ./str/file_reader.c
)

target_include_directories(my_core PUBLIC include)


#进入子目录继续编译
add_executable(my_app app/main.c)
add_executable(search
    app/main.c
    src/file_reader.c
    src/indexer.c
    src/query_processor.c
    src/ranker.c
    src/benchmark.c   # <--- 新增这一行
)
target_link_libraries(my_app PRIVATE my_core)
#“封装复用”不取决于 SHARED 还是 STATIC：
#你的 my_core 库（不管是 .dll 还是 .lib）
#通过 target_link_libraries(my_app PRIVATE my_core) 链接给 my_app 用，
#这就是标准的代码复用。my_app 只需要包含 include/ 里的头文件并链接库，就能调用 ranker.c 里的函数——这就是封装。

#“隐藏源代码”是指你把编译好的 .dll 发给客户时，不把 .c 文件发给他

#“封装复用”不取决于 SHARED 还是 STATIC
#通过 target_link_libraries(my_app PRIVATE my_core) 链接给 my_app 用，这就是标准的代码复用。
#my_app 只需要包含 include/ 里的头文件并链接库，就能调用 ranker.c 里的函数——这就是封装。

