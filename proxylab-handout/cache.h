#ifndef CACHE
#define CACHE

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000  // about 1MB
#define MAX_OBJECT_SIZE 102400
#define NCACHE_LINE 10 

typedef struct {
    int flag;           // 有效位
    int reader_cnt;     // 读取进程数
    char hdr[MAXLINE];  // HTTP响应 头部信息
    char uri[MAXLINE];
    char web_obj[MAX_OBJECT_SIZE];
    sem_t reader_cnt_mutex;
    sem_t write;
}Cache_line;

typedef struct {
    int valid_cache_lines;    // 有效的 cache 行个数
    sem_t mutex;
    Cache_line cache[NCACHE_LINE];
}Cache;

struct Node {
    int index;
    struct Node *pre;
    struct Node *next;
};

typedef struct Doublelinked_list {
    sem_t mutex;
    struct Node *head;
    struct Node *rear;
}Dbl;

// Cache functions
void init_cache(Cache *p);
int insert_cache(Cache_line *pl, Cache *p);
void update_cache(int index, Cache_line *pl, Cache *p);
int find_cache(const char *uri, Cache *p);

// LRU
void init_list(Dbl *p);
void insert_list(int index, Dbl *p);
int remove_list(Dbl *p);
void update_list(int index, Dbl *p);

#endif