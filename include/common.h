
#ifndef COMMON_H
#define COMMON_H

#include "uthash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 【重要】DLL导出宏（告诉Windows哪些函数要暴露出来供别人调用）
#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif

// 文档结构体（描述一个文本文件）
typedef struct {
    int doc_id;           // 文档编号（从0开始）
    char* file_name;      // 文件名
    char* content;        // 文件里的全部文本内容（读进来放着）
} Document;

// 倒排索引的“帖子”结构（记录一个词出现在哪篇文档里）
typedef struct {
    int doc_id;           // 出现在哪篇文档
    int frequency;        // 在这篇文档里出现了几次
    // 先预留，不做短语检索可以先空着
    int* positions;       // 该词在该文档中出现的位置数组（词序号，从0开始）
    int pos_count;        // 实际位置数量
    int pos_capacity;     // 当前容量（用于 realloc）
} Posting;

// 【重要】如果你打算用 uthash 做哈希索引，这个结构体是核心
// 每个词对应一个这样的结构体
typedef struct {
    char* word;           // 关键词（如 "apple"）
    Posting* postings;    // 指向一个动态数组，存该词出现在哪些文档里
    int postings_count;   // 数组大小
    int postings_capacity; // 当前容量（用于 realloc）
    // 如果用 uthash，下面这行是必须的（UT_hash_handle是宏定义）
    UT_hash_handle hh; 
} IndexEntry;

#endif

