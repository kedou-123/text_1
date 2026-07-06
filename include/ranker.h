#ifndef RANKER_H
#define RANKER_H

#include "common.h"

// D同学的任务：根据索引和查询词，计算每篇文档得分，返回Top-K
// 参数：index 是B同学建好的索引
// 参数：docs 是A同学读出来的文档列表
// 参数：doc_count 是文档总数
// 参数：tokens 是C同学切分好的词数组
// 参数：token_count 是词的数量
// 参数：top_k 是要返回前几名
// 返回值：返回排序后的文档ID数组（按分数从高到低）
EXPORT int* search_and_rank(IndexEntry* index, Document* docs, int doc_count, char** tokens, int token_count, int top_k);

// D同学的输出函数：把结果打印到屏幕上
EXPORT void print_results(int* doc_ids, int top_k, Document* docs);

// D同学的善后工作：释放结果内存
EXPORT void free_result(int* doc_ids);

#endif