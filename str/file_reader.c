#include "../include/file_reader.h"
#include <dirent.h>      // 用于遍历文件夹
#include <sys/stat.h>    // 用于判断文件类型

// 【核心函数】读取文件夹下所有 .txt 文件
EXPORT Document* read_all_files(const char* folder_path, int* doc_count) {
    // 第1步：打开文件夹
    DIR* dir=opendir(folder_path);
    if (dir==NULL) {
        printf("错误：无法打开文件夹 %s\n", folder_path);
        *doc_count=0;
        return NULL;
    }

    // 第2步：先遍历一遍，数一数有多少个 .txt 文件（为了分配精确大小的数组）
    struct dirent* entry;
    int file_num = 0;
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 "." 和 ".." 目录
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        // 检查文件名是否以 .txt 结尾
        char* ext = strstr(entry->d_name, ".txt");
        if (ext != NULL && strcmp(ext, ".txt") == 0) {
            file_num++;
        }
    }
    // 如果没找到任何 .txt 文件
    if (file_num == 0) {
        printf("警告：文件夹 %s 中没有 .txt 文件\n", folder_path);
        closedir(dir);
        *doc_count = 0;
        return NULL;
    }

    // 第3步：分配 Document 数组（精确大小）
    Document* docs = (Document*)malloc(file_num * sizeof(Document));
    if (docs == NULL) {
        printf("错误：内存分配失败\n");
        closedir(dir);
        *doc_count = 0;
        return NULL;
    }

    // 第4步：重置文件夹指针，准备第二次遍历（真正读取文件内容）
    rewinddir(dir);

    int idx = 0;  // 当前已读取的文件序号
    char full_path[512];  // 存放完整路径（文件夹路径 + 文件名）

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        char* ext = strstr(entry->d_name, ".txt");
        if (ext == NULL || strcmp(ext, ".txt") != 0)
            continue;

        // 构造完整文件路径
        snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, entry->d_name);

        // 第5步：打开文件，获取文件大小
        FILE* fp = fopen(full_path, "rb");  // 用二进制模式读，防止Windows下换行符干扰
        if (fp == NULL) {
            printf("警告：无法打开文件 %s，跳过\n", full_path);
            continue;
        }

        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        rewind(fp);

        // 第6步：分配内存存放文件内容（+1 是为了存结尾的 '\0'）
        char* content = (char*)malloc((file_size + 1) * sizeof(char));
        if (content == NULL) {
            printf("警告：内存不足，跳过文件 %s\n", full_path);
            fclose(fp);
            continue;
        }

        // 第7步：读入整个文件内容
        size_t bytes_read = fread(content, 1, file_size, fp);
        content[bytes_read] = '\0';  // 手动添加字符串结束符

        fclose(fp);

        // 第8步：把文档信息存入结构体
        docs[idx].doc_id = idx;
        // 复制文件名（因为 entry->d_name 是临时的，要自己存一份）
        docs[idx].file_name = (char*)malloc((strlen(entry->d_name) + 1) * sizeof(char));
        strcpy(docs[idx].file_name, entry->d_name);
        docs[idx].content = content;  // 直接指向刚刚分配的内容

        idx++;
    }

    // 第9步：关闭文件夹
    closedir(dir);

    // 第10步：返回实际读到的文件数量
    *doc_count = idx;
    return docs;
}

// 【善后函数】释放所有文档占用的内存
EXPORT void free_documents(Document* docs, int doc_count) {
    if (docs == NULL) return;
    for (int i = 0; i < doc_count; i++) {
        free(docs[i].file_name);   // 释放文件名
        free(docs[i].content);     // 释放文件内容
    }
    free(docs);  // 释放数组本身
}