#include "benchmark.h"
#include "query_processor.h"
#include "ranker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ---------- 跨平台高精度计时（单位：微秒） ----------
#ifdef _WIN32
#include <windows.h>
static double get_time_us() {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / (double)freq.QuadPart * 1000000.0;
}
#else
#include <time.h>
static double get_time_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}
#endif

// ---------- 排序比较函数（用于统计分位数） ----------
static int cmp_double(const void* a, const void* b) {
    double da = *(const double*)a, db = *(const double*)b;
    return (da > db) - (da < db);
}

// ---------- 公共函数：执行基准测试 ----------
EXPORT void run_benchmark(const char* query_file, IndexEntry* index,
                          Document* docs, int doc_count, int top_k) {
    if (!query_file || !index || !docs || doc_count <= 0) {
        printf("Benchmark 错误：参数无效。\n");
        return;
    }

    // 1. 打开查询文件
    FILE* fp = fopen(query_file, "r");
    if (!fp) {
        printf("Benchmark 错误：无法打开查询文件 '%s'\n", query_file);
        return;
    }

    // 2. 第一遍遍历：统计总查询数，并存储所有查询行（动态数组）
    char** query_lines = NULL;
    int query_count = 0;
    char line[512];

    while (fgets(line, sizeof(line), fp)) {
        // 去除末尾换行符和回车
        size_t len = strlen(line);
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[len-1] = '\0';
            len--;
        }
        if (len > 0 && line[len-1] == '\r') {
            line[len-1] = '\0';
            len--;
        }
        // 跳过空行
        if (len == 0) continue;

        query_lines = (char**)realloc(query_lines, sizeof(char*) * (query_count + 1));
        query_lines[query_count] = strdup(line);
        query_count++;
    }
    fclose(fp);

    if (query_count == 0) {
        printf("Benchmark 警告：查询文件为空，没有执行任何测试。\n");
        return;
    }

    // 3. 准备存储每次查询耗时的数组（微秒）
    double* times = (double*)malloc(sizeof(double) * query_count);
    if (!times) {
        printf("Benchmark 错误：内存分配失败。\n");
        for (int i=0; i<query_count; i++) free(query_lines[i]);
        free(query_lines);
        return;
    }

    printf("\n========== 开始 Benchmark 性能测试 ==========\n");
    printf("总查询数: %d\n", query_count);
    printf("正在执行... (请稍候)\n");

    // 4. 循环执行每条查询（静默模式，不打印结果）
    for (int i = 0; i < query_count; i++) {
        char* raw_query = query_lines[i];
        if (!raw_query) continue;

        // 判断是否为短语（首尾带双引号）
        int len = strlen(raw_query);
        int is_phrase = 0;
        char* parse_str = raw_query;
        if (len >= 2 && raw_query[0] == '"' && raw_query[len-1] == '"') {
            is_phrase = 1;
            // 去掉首尾引号，复制一份传给 parse_query
            char temp[512];
            strcpy(temp, raw_query + 1);
            temp[len - 2] = '\0';
            parse_str = temp;
        }

        // 解析查询词（无论是否为短语，都通过 parse_query 拆词）
        char** tokens = parse_query(parse_str);
        if (!tokens) continue;

        int token_count = 0;
        while (tokens[token_count] != NULL) token_count++;
        if (token_count == 0) {
            free_query_tokens(tokens);
            continue;
        }

        // ---- 计时核心区（排除解析和内存分配开销，只测检索） ----
        double start = get_time_us();
        
        int* results = NULL;
        if (is_phrase) {
            results = search_phrase(index, docs, doc_count, tokens, token_count, top_k);
        } else {
            results = search_and_rank(index, docs, doc_count, tokens, token_count, top_k);
        }
        
        double end = get_time_us();
        // ---- 计时结束 ----

        times[i] = end - start;

        // 清理本次查询的结果（释放数组，同时自动重置全局 g_match_counts）
        if (results) {
            free_result(results);
        }
        free_query_tokens(tokens);
    }

    // 5. 计算统计指标
    // 排序 times，用于计算最大/最小/分位数
    qsort(times, query_count, sizeof(double), cmp_double);

    double total_time = 0.0;
    for (int i = 0; i < query_count; i++) {
        total_time += times[i];
    }
    double avg_time = total_time / query_count;
    double min_time = times[0];
    double max_time = times[query_count - 1];
    double p95_time = times[(int)(query_count * 0.95)];
    if (p95_time == 0 && query_count > 0) p95_time = times[query_count - 1]; // 安全兜底

    // 6. 输出结果（单位换算：微秒 -> 毫秒/秒）
    printf("\n========== Benchmark 统计结果 ==========\n");
    printf("查询总数        : %d\n", query_count);
    printf("总耗时          : %.3f ms (%.3f s)\n", total_time / 1000.0, total_time / 1000000.0);
    printf("平均耗时        : %.3f μs (%.3f ms)\n", avg_time, avg_time / 1000.0);
    printf("最小耗时        : %.3f μs\n", min_time);
    printf("最大耗时        : %.3f μs\n", max_time);
    printf("95%% 分位耗时   : %.3f μs\n", p95_time);
    printf("=========================================\n");

    // 7. 清理内存
    for (int i = 0; i < query_count; i++) {
        free(query_lines[i]);
    }
    free(query_lines);
    free(times);
}