// ===========================================
// 文件名：src/main.c
// 作业内容：主程序，协调所有模块工作
// ===========================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 引入所有模块的“菜单”（.h文件）
#include "../include/file_reader.h"
#include "../include/indexer.h"
#include "../include/query_processor.h"
#include "../include/ranker.h"


int main() {
    printf("========== 轻量文本检索工具 ==========\n\n");

    // ---------- 第1步：读取文档 ----------
    int doc_count = 0;
    Document* docs = read_all_files("./data", &doc_count);
    if (docs == NULL || doc_count == 0) {
        printf("错误：在 ./data 文件夹下没有找到任何 .txt 文件！\n");
        printf("请确保 data 文件夹里有测试文件。\n");
        return -1;
    }
    printf("✅ 成功读取 %d 篇文档。\n\n", doc_count);

    // ---------- 第2步：构建倒排索引 ----------
    IndexEntry* index = build_index(docs, doc_count);
    if (index == NULL) {
        printf("错误：构建索引失败！\n");
        free_documents(docs, doc_count);
        return -1;
    }
    printf("✅ 索引构建完成。\n\n");

    // ---------- 第3步：进入交互查询循环 ----------
    printf("请输入关键词进行检索（输入 'exit' 或 'quit' 退出程序）\n");
    printf("提示：支持多个词用空格分隔，例如 'hello world'\n");
    printf("----------------------------------------\n");

    char input_line[512];
    while (1) {
        printf("\n>> 请输入查询: ");
        fgets(input_line, sizeof(input_line), stdin);

        // 去掉输入末尾的换行符
        size_t len = strlen(input_line);
        if (len > 0 && input_line[len - 1] == '\n') {
            input_line[len - 1] = '\0';
        }

        // 检查是否退出
        if (strcmp(input_line, "exit") == 0 || strcmp(input_line, "quit") == 0) {
            break;
        }

        // 如果输入为空，跳过本次循环
        if (strlen(input_line) == 0) {
            continue;
        }

        // ---------- 第4步：解析查询词 ----------
        char** tokens = parse_query(input_line);
        if (tokens == NULL) {
            printf("解析查询失败，请重新输入。\n");
            continue;
        }

        // 数一数一共切出了几个词
        int token_count = 0;
        while (tokens[token_count] != NULL) {
            token_count++;
        }

        if (token_count == 0) {
            printf("没有识别到有效关键词，请重新输入。\n");
            free_query_tokens(tokens);
            continue;
        }

        // ---------- 第5步：搜索并排序（取前5名） ----------
        int top_k = 5;
        int* results = search_and_rank(index,docs,doc_count, tokens, token_count, top_k);
        if (results == NULL) {
            printf("搜索失败，请重试。\n");
            free_query_tokens(tokens);
            continue;
        }

        // ---------- 第6步：打印结果 ----------
        print_results(results, top_k, docs);

        // ---------- 第7步：清理本次查询的内存 ----------
        free_query_tokens(tokens);
        free_result(results);
    }

    // ---------- 退出程序前：清理所有全局内存 ----------
    printf("\n正在清理内存...\n");
    free_documents(docs, doc_count);
    free_index(index);
    printf("程序退出成功。再见！\n");

    return 0;
}