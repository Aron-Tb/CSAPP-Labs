/*
 * mm.c - xxx malloc package.
 * 
 * 
 * 1. 空闲块组织: 如何记录空闲块
 * 2. 放置策略: 如何选择合适的空闲块，放置新块
 * 3. 分割: 如何处理空闲块的剩余部分
 * 4. 合并: 如何处理刚刚被释放的块
 * 
 * 
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "USTC",
    /* First member's full name */
    "SA22225336-谭博",
    /* First member's email address */
    "1041111561@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8  // 低3位可以编码其他信息，最低位表示分配还是空闲

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Points to first byte of heap */
static char *mem_heap;    

/* Basic constants and macros */
#define WSIZE 4            // Word and header/footer size (bytes)
#define DSIZE 8            // Double word size (bytes)
#define CHUNKSIZE (1<<12)  // Extend heap by this amount (bytes) 扩展堆默认值

#define MAX(x,y) ((x) > (y)? (x) : (y))

// Pack a size and allocated bit into a word 
#define PACK(size, alloc)  ((size) | (alloc))

// Read and write a word at address p
#define GET(p)  (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// Read the size and allocated fields from address p
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Some extra functions */
static void *extend_heap(size_t words);
static void *firstFit(size_t block_size);
static void *bestFit(size_t block_size);
static void place(void *bp, size_t payload_size);
static void *coalesce(void *bp);

/* 
 * mm_init - initialize the malloc package.
 */

// 初始化隐式空闲链表数据结构
int mm_init(void)
{
    if ((mem_heap = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;   // 分配失败
    PUT(mem_heap, 0); // 前部填充: 满足双字对齐要求
    /* 序言块 */
    PUT(mem_heap + (1*WSIZE), PACK(DSIZE, 1));
    PUT(mem_heap + (2*WSIZE), PACK(DSIZE, 1)); 
    /* 结尾块 */
    PUT(mem_heap + (3*WSIZE), PACK(0, 1));
    
    mem_heap += 2 * WSIZE; // 移动到序言块header后

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

void *mm_malloc(size_t payload_size)
{
    // 根据大小申请新的空闲块，若不存在则扩展堆空间
    // playload需要满足对齐要求：8bytes
    
    if (payload_size == 0)
        return NULL;
    char *bp = NULL;
    if (payload_size <= DSIZE)
        payload_size = DSIZE;  // 填充
    else
        payload_size = ALIGN(payload_size);
    size_t block_size = payload_size + DSIZE;
    // 首次适配
    bp = firstFit(block_size);
    // 最佳适配
    // bp = bestFit(block_size);
    if (bp)
        place(bp, payload_size);
    else  {
        size_t extend_size = MAX(CHUNKSIZE, block_size);  // 已对齐
        bp = extend_heap(extend_size/WSIZE);
        if (bp) 
            place(bp, payload_size);
    }
    return bp;

}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if (ptr == NULL)
        return ;
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);   
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

// extend_heap 向操作系统申请更多的空间，调用时机：初始化分配器时、调用 mm_malloc 不存在可用空闲块时
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    // 申请新的空闲块
    size = ALIGN(words*WSIZE);  // 满足对齐要求
    if ( (bp = mem_sbrk(size)) == (char *)-1 )  // 第一次扩充时，brk指向结尾块，mem_sbrk返回旧 brk指针
        return NULL;
    PUT(HDRP(bp), PACK(size, 0));  // 新空闲块写入header
    PUT(FTRP(bp), PACK(size, 0));  // 新空闲块写入footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  // 添加新的结尾块

    // 合并相邻空闲块如果存在
    return coalesce(bp);
}

/* 放置策略：首次适配，最佳适配 */
// 首次适配
static void *firstFit(size_t block_size)
{
// block_size包括了header和footer大小
    void *bp;
    for (bp = mem_heap; GET_SIZE(HDRP(bp))>0; bp=NEXT_BLKP(bp)) 
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= block_size) 
            return bp;
    return NULL;
}

// 最佳适配
static void *bestFit(size_t block_size)
{
    void *bp;
    void *min_bp = NULL;
    for (bp = mem_heap; GET_SIZE(HDRP(bp))>0; bp=NEXT_BLKP(bp)) 
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= block_size)
            if ( !min_bp || (GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(min_bp))) )
                min_bp = bp;
    return min_bp;
}

// 放置+分割： place分割空闲块
static void place(void *bp, size_t payload_size)
{
// 分配策略保证dp块一定大于 block_size
// 对于dp指向的空闲块进行分割
    size_t free_size = GET_SIZE(HDRP(bp));
    size_t block_size = ALIGN(payload_size) + DSIZE;
    // 判断能否分割, 被分割后的小部分需要能标注 header 和 footer
    const int left_size = free_size - block_size;
    PUT(HDRP(bp), PACK(block_size, 1));
    PUT(FTRP(bp), PACK(block_size, 1));
    bp = NEXT_BLKP(bp);
    if (left_size >= 2*DSIZE)  {
    // 分割后的payload至少为DSIZE，满足对齐要求
        PUT(HDRP(bp), PACK(left_size, 0));
        PUT(FTRP(bp), PACK(left_size, 0));
    }
    else  if (left_size == DSIZE) {
    // 因为满足双字对齐要求，所以外部碎片为 DSIZE=8
        // 标记外部碎片
        PUT(HDRP(bp), PACK(DSIZE, 1));
        PUT(FTRP(bp), PACK(DSIZE, 1)); 
    }
}

// 合并： coalesce 合并相邻的空闲块
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));  
    size_t size = GET_SIZE(HDRP(bp));
    // case 1: 不存在相邻空闲块
    if (prev_alloc && next_alloc)  { 
        return bp;
    }
    // case 2: 前一个已分配，后一个为空闲块
    else if (prev_alloc && !next_alloc)  {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // case 3: 前一个为空闲块， 后一个已分配
    else if (!prev_alloc && next_alloc)  {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // case 4: 前后都是空闲块
    else  {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); 
        bp = PREV_BLKP(bp); 
    }
    return bp;
}
