// ===========================================
// 文件名：src/indexer.c
// 作业内容：对文档拆词，统计词频，构建倒排索引（含位置信息）
// ===========================================

#include "indexer.h"
#include "uthash.h"          // 使用 uthash 进行哈希索引



// --- 2. 文档内临时聚合结构体 ---
typedef struct {
    char word[256];          // 单词副本（小写）
    int* positions;          // 动态数组，存储该词在本文档中的位置
    int pos_count;
    int pos_capacity;
} LocalTerm;

// --- 3. 辅助函数：转小写 ---
static void to_lowercase(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

// --- 4. 辅助函数：停用词判断 ---
static int is_stopword(const char* word) {
    static const char* stopwords[] = {
        "the", "a", "an", "is", "are", "was", "were",
        "and", "or", "but", "of", "for", "on", "at", "to", "in"
    };
    int count = sizeof(stopwords) / sizeof(stopwords[0]);
    for (int i = 0; i < count; i++) {
        if (strcmp(word, stopwords[i]) == 0) return 1;
    }
    return 0;
}

// ===========================================
// 【辅助函数】在文档临时词条中添加一个单词的位置
// ===========================================
static int add_local_term(LocalTerm** terms, int* count, int* capacity,
                          const char* word, int pos) {
    // 查找是否已存在
    int found = -1;
    for (int i = 0; i < *count; i++) {
        if (strcmp((*terms)[i].word, word) == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        // 新建词条
        if (*count >= *capacity) {
            *capacity *= 2;
            *terms = (LocalTerm*)realloc(*terms, sizeof(LocalTerm) * (*capacity));
            if (!*terms) return -1;
        }
        LocalTerm* new_term = &(*terms)[*count];
        strcpy(new_term->word, word);
        new_term->pos_capacity = 4;
        new_term->positions = (int*)malloc(sizeof(int) * new_term->pos_capacity);
        if (!new_term->positions) return -1;
        new_term->pos_count = 0;
        // 添加第一个位置
        new_term->positions[0] = pos;
        new_term->pos_count = 1;
        (*count)++;
    } else {
        // 已有词条，追加位置
        LocalTerm* term = &(*terms)[found];
        if (term->pos_count >= term->pos_capacity) {
            term->pos_capacity *= 2;
            term->positions = (int*)realloc(term->positions, sizeof(int) * term->pos_capacity);
            if (!term->positions) return -1;
        }
        term->positions[term->pos_count++] = pos;
    }
    return 0;
}

/// ===========================================
// 【辅助函数】刷新缓冲区中的单词（小写化、过滤停用词、加入临时词条）
// 返回值：0 成功，-1 内存分配失败
// ===========================================
static int flush_word(char* word_buf, int* buf_idx, int* token_pos,
                      LocalTerm** terms, int* count, int* capacity) {
    if (*buf_idx == 0) return 0;  // 无单词可刷新

    word_buf[*buf_idx] = '\0';
    to_lowercase(word_buf);

    if (!is_stopword(word_buf)) {
        // 调用 add_local_term，若失败则返回 -1
        if (add_local_term(terms, count, capacity, word_buf, *token_pos) != 0) {
            return -1;
        }
    }

    // 无论是否停用词，token_pos 递增（位置计数器）
    (*token_pos)++;
    *buf_idx = 0;
    return 0;
}
// ===========================================
// 【辅助函数】向 IndexEntry 中添加一个属于新文档的 Posting
// 返回值：0 成功，-1 内存分配失败
// ===========================================
static int add_posting_to_entry(IndexEntry* entry, int doc_id,
                                int* positions, int pos_count) {
    // 检查是否需要扩容 postings 数组
    if (entry->postings_count >= entry->postings_capacity) {
        entry->postings_capacity *= 2;
        entry->postings = (Posting*)realloc(entry->postings,
                                            sizeof(Posting) * entry->postings_capacity);
        if (!entry->postings) return -1;
    }

    // 创建新的 Posting
    Posting* p = &entry->postings[entry->postings_count];
    p->doc_id = doc_id;
    
    // ✅ 按实际需要分配，至少为 1（防止 pos_count = 0）
    p->pos_capacity = pos_count > 0 ? pos_count : 1;
    p->positions = (int*)malloc(sizeof(int) * p->pos_capacity);
    if (!p->positions) return -1;

    // 复制位置数据
    p->pos_count = pos_count;
    p->frequency = pos_count;
    for (int k = 0; k < pos_count; k++) {
        p->positions[k] = positions[k];
    }
    
    entry->postings_count++;
    return 0;
}
// --- 5. 构建倒排索引（含位置信息） ---
EXPORT IndexEntry* build_index(Document* docs, int doc_count) {
    if (docs == NULL || doc_count <= 0) {
        return NULL;
    }

    IndexEntry* hash_table = NULL;

    for (int doc_id = 0; doc_id < doc_count; doc_id++) {
        const char* content = docs[doc_id].content;
        if (content == NULL) continue;

        // ---- 文档内临时聚合数组 ----
        LocalTerm* temp_terms = NULL;
        int temp_capacity = 4;
        int temp_count = 0;
        temp_terms = (LocalTerm*)malloc(sizeof(LocalTerm) * temp_capacity);
        if (!temp_terms) return NULL;

        char word_buf[256];
        int buf_idx = 0;
        int token_pos = 1;   // 词序号从1开始

        // ---- 逐字符扫描 ----
        for (int i = 0; content[i] != '\0'; i++) {
            char ch = content[i];
            if (isalnum((unsigned char)ch)) {
                if (buf_idx < (int)(sizeof(word_buf) - 1)) {
                    word_buf[buf_idx++] = ch;
                }
            } else {
                // 分隔符 -> 结束一个单词
                if (flush_word(word_buf, &buf_idx, &token_pos,
                               &temp_terms, &temp_count, &temp_capacity) != 0) {
                    // 内存分配失败，释放已分配资源并返回 NULL
                    for (int t = 0; t < temp_count; t++) free(temp_terms[t].positions);
                    free(temp_terms);
                    return NULL;
                }
            }
        }

        // ---- 处理末尾单词 ----
        if (flush_word(word_buf, &buf_idx, &token_pos,
                       &temp_terms, &temp_count, &temp_capacity) != 0) {
            for (int t = 0; t < temp_count; t++) free(temp_terms[t].positions);
            free(temp_terms);
            return NULL;
        }

        // ---- 将 temp_terms 合并到全局哈希表 ----
        for (int t = 0; t < temp_count; t++) {
            LocalTerm* local = &temp_terms[t];
            IndexEntry* entry = NULL;
            HASH_FIND_STR(hash_table, local->word, entry);

            if (entry == NULL) {
                // 创建新词条
                entry = (IndexEntry*)malloc(sizeof(IndexEntry));
                if (!entry) {
                    for (int tt = 0; tt < temp_count; tt++) free(temp_terms[tt].positions);
                    free(temp_terms);
                    return NULL;
                }
                entry->word = strdup(local->word);
                if (!entry->word) {
                    free(entry);
                    for (int tt = 0; tt < temp_count; tt++) free(temp_terms[tt].positions);
                    free(temp_terms);
                    return NULL;
                }
                entry->postings_capacity = 4;
                entry->postings = (Posting*)malloc(sizeof(Posting) * entry->postings_capacity);
                if (!entry->postings) {
                    free(entry->word);
                    free(entry);
                    for (int tt = 0; tt < temp_count; tt++) free(temp_terms[tt].positions);
                    free(temp_terms);
                    return NULL;
                }
                entry->postings_count = 0;

                // 添加第一个 posting（使用辅助函数）
                if (add_posting_to_entry(entry, doc_id, local->positions, local->pos_count) != 0) {
                    free(entry->postings);
                    free(entry->word);
                    free(entry);
                    for (int tt = 0; tt < temp_count; tt++) free(temp_terms[tt].positions);
                    free(temp_terms);
                    return NULL;
                }

                HASH_ADD_STR(hash_table, word, entry);
            } else {
                // 词条已存在，检查最后一个 posting 是否属于当前文档
                int last_idx = entry->postings_count - 1;
                if (last_idx >= 0 && entry->postings[last_idx].doc_id == doc_id) {
                    // 同一文档，追加位置
                    Posting* p = &entry->postings[last_idx];
                    int new_count = p->pos_count + local->pos_count;
                    if (new_count > p->pos_capacity) {
                        while (new_count > p->pos_capacity) {
                            p->pos_capacity *= 2;
                        }
                        p->positions = (int*)realloc(p->positions, sizeof(int) * p->pos_capacity);
                        if (!p->positions) {
                            for (int tt = 0; tt < temp_count; tt++) free(temp_terms[tt].positions);
                            free(temp_terms);
                            return NULL;
                        }
                    }
                    for (int k = 0; k < local->pos_count; k++) {
                        p->positions[p->pos_count + k] = local->positions[k];
                    }
                    p->pos_count += local->pos_count;
                    p->frequency = p->pos_count;
                } else {
                    // 新文档，新增 posting（使用辅助函数）
                    if (add_posting_to_entry(entry, doc_id, local->positions, local->pos_count) != 0) {
                        for (int tt = 0; tt < temp_count; tt++) free(temp_terms[tt].positions);
                        free(temp_terms);
                        return NULL;
                    }
                }
            }
        }

        // ---- 释放 temp_terms ----
        for (int t = 0; t < temp_count; t++) {
            free(temp_terms[t].positions);
        }
        free(temp_terms);
    }

    return hash_table;
}
// --- 6. 释放索引内存（由内向外） ---
EXPORT void free_index(IndexEntry* index) {
    if (index == NULL) return;

    IndexEntry *current, *tmp;
    HASH_ITER(hh, index, current, tmp) {
        // 释放所有 posting 中的 positions 数组
        for (int i = 0; i < current->postings_count; i++) {
            Posting* p = &current->postings[i];
            if (p->positions) {
                free(p->positions);
            }
        }
        // 释放 postings 数组本身
        if (current->postings) {
            free(current->postings);
        }
        // 释放 word
        if (current->word) {
            free(current->word);
        }
        // 从哈希表中删除
        HASH_DEL(index, current);
        // 释放当前 IndexEntry
        free(current);
    }
}