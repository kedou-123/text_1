#ifndef INDEXER_H
#define INDEXER_H

#include "common.h"

// B同学的任务：把A同学读出来的文档，拆词、统计词频，建成倒排索引
// 参数：docs 是A同学传过来的文档数组
// 参数：doc_count 是文档数量
// 返回值：返回倒排索引（这里我们用结构体数组表示，具体实现B同学自己定）
EXPORT IndexEntry* build_index(Document* docs, int doc_count);

// B同学的善后工作：释放索引占用的内存
EXPORT void free_index(IndexEntry* index);

#endif