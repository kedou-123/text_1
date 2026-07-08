#include "ranker.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// 【静态全局变量】用于存储每个文档的匹配词数
// 这样 print_results 可以获取到匹配词数信息
static int* g_match_counts = NULL;  // 全局匹配词数数组
static int g_top_k = 0;             // 当前 top_k 值


// 【辅助结构】用于 qsort 排序时携带 (doc_id, score, total_freq)
typedef struct {
    int doc_id;
    double score;
    int total_freq;       // 所有匹配词在文档中的总出现次数（词频之和）
} ScoreItem;


// 【辅助函数】qsort 的比较函数（降序：分数高的排前面）
static int compare_score(const void* a, const void* b) {
    ScoreItem* sa = (ScoreItem*)a;
    ScoreItem* sb = (ScoreItem*)b;
    if (sa->score < sb->score) return 1;
    if (sa->score > sb->score) return -1;
    return 0;
}

// 【辅助函数】在某个 IndexEntry 中查找指定 doc_id 的 Posting（短语搜索用）
static Posting* find_posting_in_entry(IndexEntry* entry, int doc_id) {
    if (!entry) return NULL;
    for (int i = 0; i < entry->postings_count; i++) {
        if (entry->postings[i].doc_id == doc_id) {
            return &entry->postings[i];
        }
        if (entry->postings[i].doc_id > doc_id) break;
    }
    return NULL;
}

// 【辅助函数】在有序 int 数组中二分查找目标值（位置验证用）
static int binary_search_positions(int* arr, int len, int target) {
    int left = 0, right = len - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        if (arr[mid] == target) return 1;
        else if (arr[mid] < target) left = mid + 1;
        else right = mid - 1;
    }
    return 0;
}

// 【公共函数1】将查询词转为小写
static char** normalize_tokens(char** tokens, int token_count) {
    if (!tokens || token_count <= 0) return NULL;
    
    char** lower_tokens = (char**)malloc(sizeof(char*) * token_count);
    if (!lower_tokens) return NULL;
    
    for (int i = 0; i < token_count; i++) {
        lower_tokens[i] = strdup(tokens[i]);
        if (!lower_tokens[i]) {
            for (int j = 0; j < i; j++) free(lower_tokens[j]);
            free(lower_tokens);
            return NULL;
        }
        for (char* p = lower_tokens[i]; *p; p++) *p = tolower(*p);
    }
    return lower_tokens;
}

// 【公共函数2】释放小写化的查询词
static void free_normalized_tokens(char** lower_tokens, int token_count) {
    if (!lower_tokens) return;
    for (int i = 0; i < token_count; i++) {
        if (lower_tokens[i]) free(lower_tokens[i]);
    }
    free(lower_tokens);
}

// 【公共函数3】创建空结果数组（全填充 -1）
static int* create_empty_result(int top_k) {
    int* result = (int*)malloc(sizeof(int) * top_k);
    if (!result) return NULL;
    for (int i = 0; i < top_k; i++) {
        result[i] = -1;
    }
    return result;
}

// 【公共函数4】动态数组扩容（用于 ScoreItem）
static int expand_score_array(ScoreItem** scores, int* capacity) {
    int new_capacity = (*capacity) * 2;
    ScoreItem* new_scores = (ScoreItem*)realloc(*scores, sizeof(ScoreItem) * new_capacity);
    if (!new_scores) return 0;
    *scores = new_scores;
    *capacity = new_capacity;
    return 1;
}

// 【公共函数5】初始化匹配文档标记数组
static int* init_doc_marker(int doc_count) {
    return (int*)calloc(doc_count, sizeof(int));
}

// 【公共函数6】添加匹配文档到 ScoreItem 数组
static int add_matched_doc(ScoreItem** scores, int* match_count, int* capacity, int doc_id) {
    if (*match_count >= *capacity) {
        if (!expand_score_array(scores, capacity)) {
            return 0;
        }
    }
    (*scores)[*match_count].doc_id = doc_id;
    (*scores)[*match_count].score = 0.0;
    (*scores)[*match_count].total_freq = 0;  // 初始化为0
    (*match_count)++;
    return 1;
}

// 【函数1】单词搜索（基于倒排索引，TF 累加），返回 Top-K 文档 ID
EXPORT int* search_and_rank(IndexEntry* index, Document* docs, int doc_count,
                            char** tokens, int token_count, int top_k) {
    if (!index || !docs || doc_count <= 0 || !tokens || token_count <= 0 || top_k <= 0) {
        return NULL;
    }

    // ---- 1. 将查询词转为小写 ----
    char** lower_tokens = normalize_tokens(tokens, token_count);
    if (!lower_tokens) return NULL;

    // ---- 2. 初始化匹配文档收集 ----
    int max_matches = 1024;
    ScoreItem* matched_scores = (ScoreItem*)malloc(sizeof(ScoreItem) * max_matches);
    if (!matched_scores) {
        free_normalized_tokens(lower_tokens, token_count);
        return NULL;
    }
    int match_count = 0;
    
    int* doc_marked = init_doc_marker(doc_count);
    if (!doc_marked) {
        free(matched_scores);
        free_normalized_tokens(lower_tokens, token_count);
        return NULL;
    }

    // ---- 3. 收集所有包含查询词的文档 ----
    for (int i = 0; i < token_count; i++) {
        IndexEntry* entry = NULL;
        HASH_FIND_STR(index, lower_tokens[i], entry);
        if (!entry) continue;
        
        for (int j = 0; j < entry->postings_count; j++) {
            int doc_id = entry->postings[j].doc_id;
            if (!doc_marked[doc_id]) {
                if (!add_matched_doc(&matched_scores, &match_count, &max_matches, doc_id)) {
                    free(doc_marked);
                    free(matched_scores);
                    free_normalized_tokens(lower_tokens, token_count);
                    return NULL;
                }
                doc_marked[doc_id] = 1;
            }
        }
    }

    // ---- 4. 如果没有匹配的文档 ----
    if (match_count == 0) {
        int* result = create_empty_result(top_k);
        free(doc_marked);
        free(matched_scores);
        free_normalized_tokens(lower_tokens, token_count);
        
        // 清空全局匹配词数
        if (g_match_counts) {
            free(g_match_counts);
            g_match_counts = NULL;
        }
        g_top_k = top_k;
        return result;
    }

    // ---- 5. 计算匹配文档的分数和词频总和 ----
    for (int i = 0; i < token_count; i++) {
        IndexEntry* entry = NULL;
        HASH_FIND_STR(index, lower_tokens[i], entry);
        if (!entry) continue;
        
        for (int j = 0; j < entry->postings_count; j++) {
            Posting* p = &entry->postings[j];
            int doc_id = p->doc_id;
            if (doc_marked[doc_id]) {
                for (int k = 0; k < match_count; k++) {
                    if (matched_scores[k].doc_id == doc_id) {
                        matched_scores[k].score += (double)p->frequency;
                        // 累加词频（一个词在文档中出现的总次数）
                        matched_scores[k].total_freq += p->frequency;
                        break;
                    }
                }
            }
        }
    }

    // ---- 6. 排序 ----
    qsort(matched_scores, match_count, sizeof(ScoreItem), compare_score);

    // ---- 7. 提取结果 ----
    int* result = (int*)malloc(sizeof(int) * top_k);
    if (!result) {
        free(doc_marked);
        free(matched_scores);
        free_normalized_tokens(lower_tokens, token_count);
        return NULL;
    }
    
    int valid_count = (top_k < match_count) ? top_k : match_count;
    for (int i = 0; i < valid_count; i++) {
        result[i] = matched_scores[i].doc_id;
    }
    for (int i = valid_count; i < top_k; i++) {
        result[i] = -1;
    }

    // ---- 8. 保存词频总和到全局变量（供 print_results 使用） ----
    if (g_match_counts) {
        free(g_match_counts);
        g_match_counts = NULL;
    }
    g_match_counts = (int*)malloc(sizeof(int) * top_k);
    if (g_match_counts) {
        for (int i = 0; i < valid_count; i++) {
            g_match_counts[i] = matched_scores[i].total_freq;
        }
        for (int i = valid_count; i < top_k; i++) {
            g_match_counts[i] = 0;
        }
    }
    g_top_k = top_k;

    // ---- 9. 清理 ----
    free(doc_marked);
    free(matched_scores);
    free_normalized_tokens(lower_tokens, token_count);
    
    return result;
}

// 【函数2】短语搜索（要求查询词按顺序连续出现）
EXPORT int* search_phrase(IndexEntry* index, Document* docs, int doc_count,
                          char** tokens, int token_count, int top_k) {
    if (!index || !docs || doc_count <= 0 || !tokens || token_count <= 0 || top_k <= 0) {
        return NULL;
    }

    // ---- 1. 将查询词转为小写 ----
    char** lower_tokens = normalize_tokens(tokens, token_count);
    if (!lower_tokens) return NULL;

    // ---- 2. 获取每个词的 IndexEntry ----
    IndexEntry* entries[token_count];
    int all_exist = 1;
    for (int i = 0; i < token_count; i++) {
        HASH_FIND_STR(index, lower_tokens[i], entries[i]);
        if (!entries[i]) {
            all_exist = 0;
            break;
        }
    }

    // ---- 3. 如果某个词不存在 ----
    if (!all_exist) {
        int* result = create_empty_result(top_k);
        free_normalized_tokens(lower_tokens, token_count);
        
        // 清空全局匹配词数
        if (g_match_counts) {
            free(g_match_counts);
            g_match_counts = NULL;
        }
        g_top_k = top_k;
        return result;
    }

    // ---- 4. 初始化匹配文档收集 ----
    int max_matches = 1024;
    ScoreItem* matched_scores = (ScoreItem*)malloc(sizeof(ScoreItem) * max_matches);
    if (!matched_scores) {
        free_normalized_tokens(lower_tokens, token_count);
        return NULL;
    }
    int match_count = 0;

    // ---- 5. 验证短语连续性 ----
    Posting* first_postings = entries[0]->postings;
    int first_count = entries[0]->postings_count;

    for (int i = 0; i < first_count; i++) {
        int doc_id = first_postings[i].doc_id;
        int is_candidate = 1;

        Posting* postings_for_doc[token_count];
        postings_for_doc[0] = &first_postings[i];

        for (int j = 1; j < token_count; j++) {
            Posting* p = find_posting_in_entry(entries[j], doc_id);
            if (!p) {
                is_candidate = 0;
                break;
            }
            postings_for_doc[j] = p;
        }

        if (!is_candidate) continue;

        // 验证位置连续性
        int match_count_for_doc = 0;
        int* base_positions = postings_for_doc[0]->positions;
        int base_len = postings_for_doc[0]->pos_count;

        for (int p_idx = 0; p_idx < base_len; p_idx++) {
            int start_pos = base_positions[p_idx];
            int all_match = 1;
            for (int j = 1; j < token_count; j++) {
                int target = start_pos + j;
                if (!binary_search_positions(postings_for_doc[j]->positions,
                                             postings_for_doc[j]->pos_count,
                                             target)) {
                    all_match = 0;
                    break;
                }
            }
            if (all_match) {
                match_count_for_doc++;
            }
        }

        if (match_count_for_doc > 0) {
            if (!add_matched_doc(&matched_scores, &match_count, &max_matches, doc_id)) {
                free(matched_scores);
                free_normalized_tokens(lower_tokens, token_count);
                return NULL;
            }
            matched_scores[match_count - 1].score = (double)match_count_for_doc;
            // 短语匹配时，总出现次数 = 匹配到的短语个数 × 短语长度
            matched_scores[match_count - 1].total_freq = match_count_for_doc * token_count;
        }
    }

    // ---- 6. 如果没有匹配的文档 ----
    if (match_count == 0) {
        int* result = create_empty_result(top_k);
        free(matched_scores);
        free_normalized_tokens(lower_tokens, token_count);
        
        // 清空全局匹配词数
        if (g_match_counts) {
            free(g_match_counts);
            g_match_counts = NULL;
        }
        g_top_k = top_k;
        return result;
    }

    // ---- 7. 排序 ----
    qsort(matched_scores, match_count, sizeof(ScoreItem), compare_score);

    // ---- 8. 提取结果 ----
    int* result = (int*)malloc(sizeof(int) * top_k);
    if (!result) {
        free(matched_scores);
        free_normalized_tokens(lower_tokens, token_count);
        return NULL;
    }
    
    int valid_count = (top_k < match_count) ? top_k : match_count;
    for (int i = 0; i < valid_count; i++) {
        result[i] = matched_scores[i].doc_id;
    }
    for (int i = valid_count; i < top_k; i++) {
        result[i] = -1;
    }

    // ---- 9. 保存词频总和到全局变量（供 print_results 使用） ----
    if (g_match_counts) {
        free(g_match_counts);
        g_match_counts = NULL;
    }
    g_match_counts = (int*)malloc(sizeof(int) * top_k);
    if (g_match_counts) {
        for (int i = 0; i < valid_count; i++) {
            g_match_counts[i] = matched_scores[i].total_freq;
        }
        for (int i = valid_count; i < top_k; i++) {
            g_match_counts[i] = 0;
        }
    }
    g_top_k = top_k;

    // ---- 10. 清理 ----
    free(matched_scores);
    free_normalized_tokens(lower_tokens, token_count);
    
    return result;
}

// 【函数3】打印搜索结果（显示词频总和）
EXPORT void print_results(int* doc_ids, int top_k, Document* docs) {
    if (doc_ids == NULL || docs == NULL) {
        printf("\n========== 没有结果可显示 ==========\n");
        return;
    }

    // ---- 检查是否所有结果都是 -1 ----
    int all_empty = 1;
    for (int i = 0; i < top_k; i++) {
        if (doc_ids[i] >= 0) {
            all_empty = 0;
            break;
        }
    }
    if (all_empty) {
        printf("\n========== 搜索结果 ==========\n");
        printf("❌ 没有找到包含这些关键词的文档。\n");
        printf("========================================\n");
        return;
    }

    printf("\n========== 搜索结果（Top-%d） ==========\n", top_k);
    int printed = 0;
    for (int i = 0; i < top_k; i++) {
        int id = doc_ids[i];
        if (id < 0) break;
        
        // 获取词频总和（从全局变量读取）
        int total_freq = 0;
        if (g_match_counts && i < g_top_k) {
            total_freq = g_match_counts[i];
        }
        
        printf("排名 %d: 文档 %d (文件名: %s) - 出现 %d 次\n",
               i + 1,
               id,
               docs[id].file_name,
               total_freq);
        printed++;
    }
    if (printed == 0) {
        printf("没有找到相关文档。\n");
    }
    printf("========================================\n");
}

// 【函数4】释放结果内存（同时释放全局匹配词数）
EXPORT void free_result(int* doc_ids) {
    if (doc_ids != NULL) {
        free(doc_ids);
    }
    // 释放全局匹配词数
    if (g_match_counts) {
        free(g_match_counts);
        g_match_counts = NULL;
    }
    g_top_k = 0;
}