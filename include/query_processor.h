#ifndef QUERY_PROCESSOR_H
#define QUERY_PROCESSOR_H

#include "common.h"

// C同学的任务：把用户输入的一串关键词（如 "hello world"）切分成单独的词列表
// 参数：query_str 是用户输入的原始字符串
// 返回值：返回切分好的字符串数组（比如 {"hello", "world"}）
EXPORT char** parse_query(const char* query_str);

// C同学的善后工作：释放切分出来的词数组
EXPORT void free_query_tokens(char** tokens);

// 【加分项预留】如果组长让C同学兼做短语检测，以后加个函数
// EXPORT int is_phrase_query(const char* query_str);

#endif