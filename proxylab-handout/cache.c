#include "csapp.h"
#include "cache.h"
/* Cache */

// 初始化 cache
void init_cache(Cache *p) {
    p->valid_cache_lines = 0;
    sem_init(&p->mutex, 0, 1);
    for (int i=0; i<NCACHE_LINE; ++i) {
        Cache_line *pl = &(p->cache[i]);
        pl->flag = 0;
        pl->reader_cnt = 0;
        memset(pl->hdr, 0, MAXLINE);
        memset(pl->uri, 0, MAXLINE);
        memset(pl->web_obj, 0, MAX_OBJECT_SIZE);
        sem_init(&pl->reader_cnt_mutex, 0, 1);
        sem_init(&pl->write, 0, 1);
    }
}

// 将对应的 cache_line 数据写入 cache 空行中, 返回写入的index
int insert_cache(Cache_line *pl, Cache *p) {
    P(&p->mutex);
    for (int i=0; i<NCACHE_LINE; ++i) {
        Cache_line *pl_ = &(p->cache[i]); 
        if (pl_->flag == 0) {
            memcpy(pl, pl_, sizeof(Cache_line));
            p->valid_cache_lines += 1;
            V(&p->mutex);
            return i;
       }
    }
    V(&p->mutex);
    return -1;
}

// 将cache[index]的 cache_line 更新
void update_cache(int index, Cache_line *pl, Cache *p) {
    P(&p->mutex);
    Cache_line *old = &(p->cache[index]);
    P(&old->write);
    memcpy(old, pl, sizeof(Cache_line));
    V(&old->write);
    V(&p->mutex);
}

// 根据 URI 在 cache 中查找cache_line, 返回索引
int find_cache(const char *uri, Cache *p) {
    P(&p->mutex);
    for (int i=0; i<NCACHE_LINE; ++i) {
       Cache_line *pl = &(p->cache[i]);
       if ( !strcasecmp(pl->uri, uri) && pl->flag == 1) {
            V(&p->mutex);
            return i;
       }
    }
    V(&p->mutex);
    return -1;
}

/* LRU */

void init_list(Dbl *p) {
    struct Node *head = malloc(sizeof(struct Node));
    struct Node *rear = malloc(sizeof(struct Node));
    head->index = -1;
    head->pre = NULL;
    head->next = rear;
    rear->index = -1;
    rear->pre = head;
    rear->next = NULL;
    
    p->head = head;
    p->rear = rear;
    sem_init(&p->mutex, 0, 1);
}

void insert_list(int index, Dbl *p) {
    // 始终插入尾部: Cache未满时
    struct Node *np = (struct Node *)malloc(sizeof(struct Node));
    np->index = index;
    P(&p->mutex);
    struct Node *last = p->rear->pre;
    last->next = np;
    np->pre = last;
    np->next = p->rear;
    V(&p->mutex);
}

int remove_list(Dbl *p) {
    // 始终删除头部: Cache满时
    P(&p->mutex);
    struct Node *del = p->head->next;
    p->head->next = del->next;
    del->next->pre = p->head;
    V(&p->mutex);
    int index = del->index;
    free(del);
    return index;
}

void update_list(int index, Dbl *p) {
    if (index < 0 || index >= NCACHE_LINE) 
        app_error("update_list error : no such index!");
    
    P(&p->mutex);
    struct Node *np = p->head->next;
    while (np != p->rear) {
        if (np->index == index) {
            np->pre->next = np->next;
            np->next->pre = np->pre;
            V(&p->mutex);
            free(np);
            insert_list(index, p);
            return ; 
        }
        np = np->next;
    }
    V(&p->mutex);
    app_error("update_list error : no such index!");
}
