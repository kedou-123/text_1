// ===========================================
// 文件名：src/query_processor.c
// 作业内容：实现关键词切分和内存释放
// 注意：必须 #include "query_processor.h"
// ===========================================

#include "query_processor.h"
#include <string.h>   // 用 strtok, strdup
#include <stdlib.h>   // 用 malloc, free
#include <ctype.h>    // 用 isspace

// 【函数1】把 "hello world" 拆成 {"hello", "world", NULL} 
// 注意：返回的数组最后一个元素必须是 NULL，这样主程序才知道一共有几个词
EXPORT char** parse_query(const char* query_str) {
    // 1. 防御性检查
    if (query_str == NULL || strlen(query_str) == 0) {
        char** empty = (char**)malloc(sizeof(char*));
        empty[0] = NULL;
        return empty;
    }

    // 2. 因为 strtok 会修改原字符串，所以必须复制一份
    char* temp = strdup(query_str);  // strdup 自动申请内存并复制
    if (temp == NULL) return NULL;

    // 3. 第一遍遍历：数一数一共有几个单词（用来申请数组空间）
    int count = 0;
    char* token = strtok(temp, " .,!?;:\n\t");  // 分隔符包含空格和标点
    while (token != NULL) {
        count++;
        token = strtok(NULL, " .,!?;:\n\t");
    }

    // 4. 申请存储指针的数组（多申请一个位置放 NULL）
    char** result = (char**)malloc(sizeof(char*) * (count + 1));
    if (result == NULL) {
        free(temp);
        return NULL;
    }

    // 5. 第二遍遍历：把每个单词复制一份存进去
    // 注意：strtok 已经破坏了 temp，所以要从 query_str 再复制一份新的来切
    char* temp2 = strdup(query_str);
    if (temp2 == NULL) {
        free(temp);
        free(result);
        return NULL;
    }

    int idx = 0;
    token = strtok(temp2, " .,!?;:\n\t");
    while (token != NULL) {
        result[idx] = strdup(token);  // strdup 复制单词内容
        idx++;
        token = strtok(NULL, " .,!?;:\n\t");
    }
    result[idx] = NULL;  // 最后一位放 NULL，表示结束

    // 6. 释放临时拷贝
    free(temp);
    free(temp2);

    return result;
}

// 【函数2】释放上面申请的内存（防止内存泄漏）
EXPORT void free_query_tokens(char** tokens) {
    if (tokens == NULL) return;

    // 遍历数组，直到碰到 NULL
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);   // 释放每个单词字符串
    }
    free(tokens);          // 释放指针数组本身
}