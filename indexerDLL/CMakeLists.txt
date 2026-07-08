set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(IndexerDLL SHARED
    indexer.c
)

target_compile_definitions(IndexerDLL
    PRIVATE
        INDEXER_EXPORTS   # 与 common.h 中 EXPORT 宏配合使用
)

target_include_directories(IndexerDLL
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}  

)