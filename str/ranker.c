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
// 【辅助函数】在某个 IndexEntry 中查找指定 doc_id 的 Posting（短语搜索用）
// ===========================================
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

// ===========================================
// 【辅助函数】在有序 int 数组中二分查找目标值（位置验证用）
// ===========================================
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

// ===========================================
// 【函数1】单词搜索（基于倒排索引，TF 累加），返回 Top-K 文档 ID
// ===========================================
EXPORT int* search_and_rank(IndexEntry* index, Document* docs, int doc_count,
                            char** tokens, int token_count, int top_k) {
    if (!index || !docs || doc_count <= 0 || !tokens || token_count <= 0 || top_k <= 0) {
        return NULL;
    }

    // ---- 1. 将查询词转为小写 ----
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

    // ---- 2. 分配得分数组 ----
    ScoreItem* scores = (ScoreItem*)calloc(doc_count, sizeof(ScoreItem));
    if (!scores) {
        for (int i = 0; i < token_count; i++) free(lower_tokens[i]);
        free(lower_tokens);
        return NULL;
    }
    for (int i = 0; i < doc_count; i++) {
        scores[i].doc_id = i;
        scores[i].score = 0.0;
    }

    // ---- 3. 遍历每个查询词，累加词频 ----
    for (int i = 0; i < token_count; i++) {
        IndexEntry* entry = NULL;
        HASH_FIND_STR(index, lower_tokens[i], entry);
        if (!entry) continue;
        for (int j = 0; j < entry->postings_count; j++) {
            Posting* p = &entry->postings[j];
            scores[p->doc_id].score += (double)p->frequency;
        }
    }

    // ---- 4. 排序 ----
    qsort(scores, doc_count, sizeof(ScoreItem), compare_score);

    // ---- 【新增】检查是否有任何文档得分 > 0 ----
    int has_match = 0;
    for (int i = 0; i < doc_count; i++) {
        if (scores[i].score > 0.0) {
            has_match = 1;
            break;
        }
    }

    // ---- 5. 提取结果（长度 = top_k，不足用 -1 填充） ----
    int* result = (int*)malloc(sizeof(int) * top_k);
    if (!result) {
        free(scores);
        for (int i = 0; i < token_count; i++) free(lower_tokens[i]);
        free(lower_tokens);
        return NULL;
    }

    // 【新增】如果没有匹配，全部填充 -1
    if (!has_match) {
        for (int i = 0; i < top_k; i++) {
            result[i] = -1;
        }
        free(scores);
        for (int i = 0; i < token_count; i++) free(lower_tokens[i]);
        free(lower_tokens);
        return result;
    }   //到这里

    int valid_count = (top_k < doc_count) ? top_k : doc_count;
    for (int i = 0; i < valid_count; i++) {
        result[i] = scores[i].doc_id;
    }
    for (int i = valid_count; i < top_k; i++) {
        result[i] = -1;   // 填充哨兵
    }

    // ---- 6. 清理 ----
    free(scores);
    for (int i = 0; i < token_count; i++) free(lower_tokens[i]);
    free(lower_tokens);

    return result;
}

// ===========================================
// 【函数2】短语搜索（要求查询词按顺序连续出现）
// ===========================================
EXPORT int* search_phrase(IndexEntry* index, Document* docs, int doc_count,
                          char** tokens, int token_count, int top_k) {
    if (!index || !docs || doc_count <= 0 || !tokens || token_count <= 0 || top_k <= 0) {
        return NULL;
    }

    // ---- 1. 查询词小写化 ----
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

    // 如果某个词不存在，返回全 -1 的数组（长度 top_k）
    if (!all_exist) {
        int* result = (int*)malloc(sizeof(int) * top_k);
        if (result) {
            for (int i = 0; i < top_k; i++) result[i] = -1;
        }
        for (int i = 0; i < token_count; i++) free(lower_tokens[i]);
        free(lower_tokens);
        return result;
    }

    // ---- 3. 分配得分数组 ----
    ScoreItem* scores = (ScoreItem*)calloc(doc_count, sizeof(ScoreItem));
    if (!scores) {
        for (int i = 0; i < token_count; i++) free(lower_tokens[i]);
        free(lower_tokens);
        return NULL;
    }
    for (int i = 0; i < doc_count; i++) {
        scores[i].doc_id = i;
        scores[i].score = 0.0;
    }

    // ---- 4. 以第一个词的 posting 为候选集，验证连续性 ----
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
        int match_count = 0;
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
                match_count++;
            }
        }

        if (match_count > 0) {
            scores[doc_id].score = (double)match_count;
        }
    }

    // ---- 5. 排序 ----
    qsort(scores, doc_count, sizeof(ScoreItem), compare_score);

    // ---- 6. 提取结果（长度 = top_k，不足用 -1 填充） ----
    int* result = (int*)malloc(sizeof(int) * top_k);
    if (!result) {
        free(scores);
        for (int i = 0; i < token_count; i++) free(lower_tokens[i]);
        free(lower_tokens);
        return NULL;
    }

    int valid_count = (top_k < doc_count) ? top_k : doc_count;
    for (int i = 0; i < valid_count; i++) {
        result[i] = scores[i].doc_id;
    }
    for (int i = valid_count; i < top_k; i++) {
        result[i] = -1;
    }

    // ---- 7. 清理 ----
    free(scores);
    for (int i = 0; i < token_count; i++) free(lower_tokens[i]);
    free(lower_tokens);

    return result;
}

// ===========================================
// 【函数3】打印搜索结果
// ===========================================
EXPORT void print_results(int* doc_ids, int top_k, Document* docs) {
    if (doc_ids == NULL || docs == NULL) {
        printf("\n========== 没有结果可显示 ==========\n");
        return;
    }

    // ---- 【新增】检查是否所有结果都是 -1 ----
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
    }      //到这里

    printf("\n========== 搜索结果（Top-%d） ==========\n", top_k);
    int printed = 0;
    for (int i = 0; i < top_k; i++) {
        int id = doc_ids[i];
        if (id < 0) break;  // 遇到哨兵立即停止
        printf("排名 %d: 文档 %d (文件名: %s)\n",
               i + 1,
               id,
               docs[id].file_name);
        printed++;
    }
    if (printed == 0) {
        printf("没有找到相关文档。\n");
    }
    printf("========================================\n");
}

// ===========================================
// 【函数4】释放结果内存
// ===========================================
EXPORT void free_result(int* doc_ids) {
    if (doc_ids != NULL) {
        free(doc_ids);
    }
}