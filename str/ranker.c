#include "ranker.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// ===========================================
// 【辅助结构】用于 qsort 排序时携带 (doc_id, score)
// ===========================================
typedef struct {
    int doc_id;
    double score;
} ScoreItem;

// ===========================================
// 【辅助函数】qsort 的比较函数（降序：分数高的排前面）
// ===========================================
static int compare_score(const void* a, const void* b) {
    ScoreItem* sa = (ScoreItem*)a;
    ScoreItem* sb = (ScoreItem*)b;
    if (sa->score < sb->score) return 1;
    if (sa->score > sb->score) return -1;
    return 0;
}

// ===========================================
// 【辅助函数】计算一篇文档中某个词出现了几次（词频）
// ===========================================
static int count_word_frequency(const char* content, const char* word) {
    if (content == NULL || word == NULL) return 0;
    
    char* temp = strdup(content);
    if (temp == NULL) return 0;
    
    for (int i = 0; temp[i]; i++) {
        temp[i] = tolower(temp[i]);
    }
    
    int count = 0;
    char* token = strtok(temp, " .,!?;:\n\t\r");
    while (token != NULL) {
        if (strcmp(token, word) == 0) {
            count++;
        }
        token = strtok(NULL, " .,!?;:\n\t\r");
    }
    
    free(temp);
    return count;
}

// ===========================================
// 【函数1】搜索并排序，返回 Top-K 文档 ID
// ===========================================
EXPORT int* search_and_rank(IndexEntry* index, Document* docs, int doc_count, char** tokens, int token_count, int top_k) {
    if (docs == NULL || doc_count <= 0 || tokens == NULL || token_count <= 0 || top_k <= 0) {
        return NULL;
    }
    
    ScoreItem* scores = (ScoreItem*)malloc(sizeof(ScoreItem) * doc_count);
    if (scores == NULL) return NULL;
    
    for (int doc_id = 0; doc_id < doc_count; doc_id++) {
        scores[doc_id].doc_id = doc_id;
        scores[doc_id].score = 0.0;
        
        for (int i = 0; i < token_count; i++) {
            int freq = count_word_frequency(docs[doc_id].content, tokens[i]);
            scores[doc_id].score += freq;
        }
    }
    
    qsort(scores, doc_count, sizeof(ScoreItem), compare_score);
    
    int actual_k = (top_k < doc_count) ? top_k : doc_count;
    int* result = (int*)malloc(sizeof(int) * actual_k);
    if (result == NULL) {
        free(scores);
        return NULL;
    }
    
    for (int i = 0; i < actual_k; i++) {
        result[i] = scores[i].doc_id;
    }
    
    free(scores);
    return result;
}

// ===========================================
// 【函数2】打印搜索结果
// ===========================================
EXPORT void print_results(int* doc_ids, int top_k, Document* docs) {
    if (doc_ids == NULL || docs == NULL) {
        printf("\n========== 没有结果可显示 ==========\n");
        return;
    }
    
    printf("\n========== 搜索结果（Top-%d） ==========\n", top_k);
    int printed = 0;
    for (int i = 0; i < top_k; i++) {
        if (doc_ids[i] < 0) break;
        printf("排名 %d: 文档 %d (文件名: %s)\n", 
               i + 1, 
               doc_ids[i], 
               docs[doc_ids[i]].file_name);
        printed++;
    }
    if (printed == 0) {
        printf("没有找到相关文档。\n");
    }
    printf("========================================\n");
}

// ===========================================
// 【函数3】释放结果内存
// ===========================================
EXPORT void free_result(int* doc_ids) {
    if (doc_ids != NULL) {
        free(doc_ids);
    }
}

// ===========================================
// 【测试代码】单独调试时使用
// ===========================================
#ifdef TEST_RANKER

#include <stdio.h>

int main() {
    printf("========== 开始测试 ranker 模块 ==========\n\n");
    
    // 模拟 A 同学的文档数据
    Document test_docs[4];
    
    test_docs[0].doc_id = 0;
    test_docs[0].file_name = "doc1.txt";
    test_docs[0].content = strdup("hello world this is a test document for search engine");
    
    test_docs[1].doc_id = 1;
    test_docs[1].file_name = "doc2.txt";
    test_docs[1].content = strdup("hello C programming language is very powerful");
    
    test_docs[2].doc_id = 2;
    test_docs[2].file_name = "doc3.txt";
    test_docs[2].content = strdup("world of programming and algorithms are fun");
    
    test_docs[3].doc_id = 3;
    test_docs[3].file_name = "doc4.txt";
    test_docs[3].content = strdup("this document has nothing about search or world");
    
    int doc_count = 4;
    
    // 模拟 C 同学的查询词
    char* tokens[] = {"hello", "world", NULL};
    int token_count = 2;
    
    // 模拟 B 同学的索引（暂时用不到）
    IndexEntry* fake_index = NULL;
    
    printf("查询词：hello world\n");
    printf("文档总数：%d\n\n", doc_count);
    
    int* results = search_and_rank(fake_index, test_docs, doc_count, tokens, token_count, 3);
    print_results(results, 3, test_docs);
    
    free_result(results);
    for (int i = 0; i < doc_count; i++) {
        free((void*)test_docs[i].content);
    }
    
    printf("\n========== 测试完成 ==========\n");
    return 0;
}
#endif