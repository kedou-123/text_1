// ===========================================
// 文件名：src/indexer.c
// 作业内容：对文档拆词，统计词频，构建倒排索引（含位置信息）
// ===========================================

#include "indexer.h"
#include "uthash.h"          // 使用 uthash 进行哈希索引

// --- 1. 扩展 Posting 结构体（添加位置数组） ---
// 在 indexer.c 内部定义增强版 Posting，并用宏覆盖原类型
typedef struct {
    int doc_id;
    int frequency;           // 冗余字段，可由 pos_count 获得，但保留方便
    int* positions;          // 动态数组，存储词序号（从1开始）
    int pos_count;           // 当前位置个数
    int pos_capacity;        // positions 数组容量
} PostingEx;

#define Posting PostingEx    // 此后所有 Posting 都被替换为 PostingEx

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

// --- 5. 构建倒排索引（含位置信息） ---
EXPORT IndexEntry* build_index(Document* docs, int doc_count) {
    if (docs == NULL || doc_count <= 0) {
        return NULL;
    }

    IndexEntry* hash_table = NULL;   // 全局哈希表头

    // 遍历每篇文档
    for (int doc_id = 0; doc_id < doc_count; doc_id++) {
        const char* content = docs[doc_id].content;
        if (content == NULL) continue;

        // ----- 文档内临时聚合数组 -----
        LocalTerm* temp_terms = NULL;
        int temp_capacity = 4;
        int temp_count = 0;
        temp_terms = (LocalTerm*)malloc(sizeof(LocalTerm) * temp_capacity);
        if (!temp_terms) return NULL; // 内存分配失败

        // 扫描状态
        char word_buf[256];
        int buf_idx = 0;
        int token_pos = 1;    // 词序号从1开始

        // 逐字符扫描
        for (int i = 0; content[i] != '\0'; i++) {
            char ch = content[i];
            if (isalnum((unsigned char)ch)) {
                // 字母或数字，入缓冲区（防止溢出）
                if (buf_idx < (int)(sizeof(word_buf) - 1)) {
                    word_buf[buf_idx++] = ch;
                }
            } else {
                // 分隔符 -> 结束一个单词
                if (buf_idx > 0) {
                    word_buf[buf_idx] = '\0';
                    to_lowercase(word_buf);

                    if (!is_stopword(word_buf)) {
                        // 在 temp_terms 中查找是否已有该词
                        int found = -1;
                        for (int j = 0; j < temp_count; j++) {
                            if (strcmp(temp_terms[j].word, word_buf) == 0) {
                                found = j;
                                break;
                            }
                        }

                        if (found == -1) {
                            // 新建临时词条
                            if (temp_count >= temp_capacity) {
                                temp_capacity *= 2;
                                temp_terms = (LocalTerm*)realloc(temp_terms, sizeof(LocalTerm) * temp_capacity);
                                if (!temp_terms) return NULL;
                            }
                            LocalTerm* new_term = &temp_terms[temp_count];
                            strcpy(new_term->word, word_buf);
                            new_term->pos_capacity = 4;
                            new_term->positions = (int*)malloc(sizeof(int) * new_term->pos_capacity);
                            if (!new_term->positions) return NULL;
                            new_term->pos_count = 0;
                            // 存入第一个位置
                            new_term->positions[0] = token_pos;
                            new_term->pos_count = 1;
                            temp_count++;
                        } else {
                            // 已存在，追加位置
                            LocalTerm* term = &temp_terms[found];
                            if (term->pos_count >= term->pos_capacity) {
                                term->pos_capacity *= 2;
                                term->positions = (int*)realloc(term->positions, sizeof(int) * term->pos_capacity);
                                if (!term->positions) return NULL;
                            }
                            term->positions[term->pos_count++] = token_pos;
                        }
                    }
                    // 词序号递增（每个有效词都计数，包括停用词？说明书：记录的是词序号，有效词才递增吗？
                    // 说明书说 token_pos 是词序号计数器，每篇文档重置为1，遇到有效词才递增？
                    // 实际上，按照说明书步骤二：“token_pos++（准备记录下一个词的位置）”，说明每个词（无论是否停用词？）
                    // 但说明书是在通过停用词过滤后才 token_pos++，意味着停用词也占位置，但我们不记录停用词。
                    // 所以 token_pos 应该每个单词都递增，而不仅仅有效词。
                    // 但说明书说“遇到分隔符且 buf_idx > 0，即一个单词结束”，然后判断是否停用词，若通过则记录当前 token_pos，然后 token_pos++。
                    // 所以 token_pos 应该在每个单词结束时递增（无论是否停用词），因为位置是词序号，所有词都算。
                    // 我这里将 token_pos++ 放在 if (!is_stopword) 外面，即每个单词都递增。
                    token_pos++;
                    buf_idx = 0;
                }
            }
        }
        // 处理末尾可能的最后一个单词
        if (buf_idx > 0) {
            word_buf[buf_idx] = '\0';
            to_lowercase(word_buf);
            if (!is_stopword(word_buf)) {
                // 查找并添加
                int found = -1;
                for (int j = 0; j < temp_count; j++) {
                    if (strcmp(temp_terms[j].word, word_buf) == 0) {
                        found = j;
                        break;
                    }
                }
                if (found == -1) {
                    if (temp_count >= temp_capacity) {
                        temp_capacity *= 2;
                        temp_terms = (LocalTerm*)realloc(temp_terms, sizeof(LocalTerm) * temp_capacity);
                        if (!temp_terms) return NULL;
                    }
                    LocalTerm* new_term = &temp_terms[temp_count];
                    strcpy(new_term->word, word_buf);
                    new_term->pos_capacity = 4;
                    new_term->positions = (int*)malloc(sizeof(int) * new_term->pos_capacity);
                    if (!new_term->positions) return NULL;
                    new_term->pos_count = 0;
                    new_term->positions[0] = token_pos;
                    new_term->pos_count = 1;
                    temp_count++;
                } else {
                    LocalTerm* term = &temp_terms[found];
                    if (term->pos_count >= term->pos_capacity) {
                        term->pos_capacity *= 2;
                        term->positions = (int*)realloc(term->positions, sizeof(int) * term->pos_capacity);
                        if (!term->positions) return NULL;
                    }
                    term->positions[term->pos_count++] = token_pos;
                }
            }
            token_pos++; // 末尾单词也递增（但之后无用了）
        }

        // ----- 将 temp_terms 合并到全局哈希表 -----
        for (int t = 0; t < temp_count; t++) {
            LocalTerm* local = &temp_terms[t];
            IndexEntry* entry = NULL;
            HASH_FIND_STR(hash_table, local->word, entry);

            if (entry == NULL) {
                // 创建新词条
                entry = (IndexEntry*)malloc(sizeof(IndexEntry));
                if (!entry) return NULL;
                entry->word = strdup(local->word);
                if (!entry->word) return NULL;

                entry->postings_capacity = 4;
                entry->postings = (Posting*)malloc(sizeof(Posting) * entry->postings_capacity);
                if (!entry->postings) return NULL;
                entry->postings_count = 0;

                // 创建第一个 posting（当前文档）
                Posting* p = &entry->postings[entry->postings_count];
                p->doc_id = doc_id;
                p->pos_capacity = 4;
                p->positions = (int*)malloc(sizeof(int) * p->pos_capacity);
                if (!p->positions) return NULL;
                // 复制位置
                p->pos_count = local->pos_count;
                p->frequency = local->pos_count;
                for (int k = 0; k < local->pos_count; k++) {
                    p->positions[k] = local->positions[k];
                }
                entry->postings_count++;

                // 插入哈希表
                HASH_ADD_STR(hash_table, word, entry);
            } else {
                // 词条已存在，检查最后一个 posting 是否属于当前文档
                int last_idx = entry->postings_count - 1;
                if (last_idx >= 0 && entry->postings[last_idx].doc_id == doc_id) {
                    // 同一文档，追加位置到该 posting
                    Posting* p = &entry->postings[last_idx];
                    int new_count = p->pos_count + local->pos_count;
                    if (new_count > p->pos_capacity) {
                        while (new_count > p->pos_capacity) {
                            p->pos_capacity *= 2;
                        }
                        p->positions = (int*)realloc(p->positions, sizeof(int) * p->pos_capacity);
                        if (!p->positions) return NULL;
                    }
                    for (int k = 0; k < local->pos_count; k++) {
                        p->positions[p->pos_count + k] = local->positions[k];
                    }
                    p->pos_count += local->pos_count;
                    p->frequency = p->pos_count; // 更新频率
                } else {
                    // 新文档，新增 posting
                    if (entry->postings_count >= entry->postings_capacity) {
                        entry->postings_capacity *= 2;
                        entry->postings = (Posting*)realloc(entry->postings, sizeof(Posting) * entry->postings_capacity);
                        if (!entry->postings) return NULL;
                    }
                    Posting* p = &entry->postings[entry->postings_count];
                    p->doc_id = doc_id;
                    p->pos_capacity = 4;
                    p->positions = (int*)malloc(sizeof(int) * p->pos_capacity);
                    if (!p->positions) return NULL;
                    p->pos_count = local->pos_count;
                    p->frequency = local->pos_count;
                    for (int k = 0; k < local->pos_count; k++) {
                        p->positions[k] = local->positions[k];
                    }
                    entry->postings_count++;
                }
            }
        }

        // ----- 释放 temp_terms 内部的动态数组和自身 -----
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