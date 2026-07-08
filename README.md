项目简介：Mini Text Search Engine 是一个基于 C 语言的命令行文本检索工具。输入一批文本文件后自动建立索引，支持关键词检索并返回最相关的 Top-K 文档。
环境要求：需安装C语言编译器（GCC 8.0+版本），确保开发环境依赖完整。
快速开始：将源码下载至本地，进入项目根目录，编译命令“gcc -Iinclude str\file_reader.c str\indexer.c str\query_processor.c str\ranker.c app\main.c -o search.exe”，运行“.\search.exe --index data”，即可启动程序。
项目目录结构：text_1/
            ├── include/            # 头文件目录
            │   ├── common.h       # 公共结构体定义（Document / IndexEntry / Posting）
            │   ├── file_reader.h  # 文件读取模块接口
            │   ├── indexer.h      # 索引构建模块接口
            │   ├── query_processor.h  # 查询分词模块接口
            │   ├── ranker.h       # 排序输出模块接口
            │   └── uthash.h       # 第三方哈希库
            ├── str/               # 源文件目录（各模块实现）
            │   ├── file_reader.c  # 文件读取与预处理
            │   ├── indexer.c      # 倒排索引构建
            │   ├── query_processor.c  # 查询分词与短语检测
            │   └── ranker.c       # BM25排序与结果输出
            ├── app/               # 主程序目录
            │   └── main.c         # 程序入口，串联各模块
            ├── data/              # 测试数据目录
            │   ├── data_1         # 测试文档集1
            │   ├── data_2         # 测试文档集2
            │   └── data           # 测试文档集3
            └── CMakeLists.txt     # CMake构建配置文件
