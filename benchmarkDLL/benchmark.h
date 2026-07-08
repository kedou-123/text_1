#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "common.h"

/**
 * 执行批量性能基准测试
 * @param query_file   查询文件路径（每行一个查询，支持带引号的短语）
 * @param index        已构建的倒排索引
 * @param docs         文档数组
 * @param doc_count    文档总数
 * @param top_k        返回 Top-K 结果数
 */
EXPORT void run_benchmark(const char* query_file, IndexEntry* index, 
                          Document* docs, int doc_count, int top_k);

#endif