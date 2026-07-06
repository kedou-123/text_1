#ifndef FILE_READER_H
#define FILE_READER_H

#include "common.h"

// A同学的任务：去 data/ 文件夹下把所有 .txt 文件读进来
// 参数：folder_path 是文件夹路径（如 "./data"）
// 参数：doc_count 是个指针，用来返回读到了多少个文件
// 返回值：Document 数组的首地址
EXPORT Document* read_all_files(const char* folder_path, int* doc_count);

// A同学的善后工作：释放读出来的文档内存
EXPORT void free_documents(Document* docs, int doc_count);

#endif