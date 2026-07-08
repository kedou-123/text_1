add_library(RankerDLL SHARED
    ranker.c
)

target_compile_definitions(RankerDLL
    PRIVATE
        RANKER_EXPORTS   # 与 common.h 中 EXPORT 宏配合使用
)

target_include_directories(RankerDLL
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}  

)