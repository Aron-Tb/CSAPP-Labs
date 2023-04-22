// Author: Aron
// Date: 4/22/2023

/* requirements：
 * 1. compile csim.c without warnings.
 * 2. arbitrary s, E and b. use malloc function. 
 * 3. only interested in data cache performance, ignore all instruction cache access(line starting with "I")
 *    vargrind always put "I" in the first column (no preceding space). "M", "L", and "S" in th second column (with a       preceding space).
 * 4. must call function printSummary, with the total number of hits, misses, and evictions, at the end of your main f      unction: 
              printSummary(hit_count, miss_count, eviction_count);
 * 5. assume memory accesses are aligned properly. (never crosses block boundaries)
 * 6. outputting the correct number of cache hits, misses and evictions give full credit. 
 * 7. uses the LRU (least-recently used) replacement policy when choosing which cache line to evict.
 */ 

/* hints and suggestions:
 * 1. initial debugging on the samll traces.
 * 2. implement -v feature in csim.c code.
 * 3. use the getopt function to parse command line arguments.
 * 4. L or S operation can cause at most one cache miss. 
 *    M operation can cause two cache hits, or a miss and hit plus a possible eviction.
 */ 

#include "getopt.h"
#include "stdlib.h"
#include "unistd.h"

#include "cachelab.h"

typedef struct cache_line
{
	// 此结构并没有放入真正的内存数据副本，仅做模拟评估。
	char valid; // 有效位
	int tag; // 标记位
    int	time_stamp; // 时间戳, LRU使用 
}Cache_line;

typedef struct cache
{
	int S;
	int E;
	int B;	
	Cache_line **line;
}Cache;

Cache *cache = 0;
static int hit_count = 0, miss_count = 0, eviction_count = 0;

Cache *init_cache(int s, int E, int b)
{
	if (s>>31 || E>>31 || b>>31) 
		return 0; // 非法输入
	int S = 1<<s;
	int B = 1<<b;
	cache = (Cache *)malloc(sizeof(Cache));
	if (!cache) 
		return 0; // 初始化失败
	cache->S = S;
	cache->E = E;
	cache->B = B;
	cache->line = (Cache_line *)malloc(sizeof(Cache_line) * S * E);	
	if (!cache->line) 
		return 0; // 初始化失败
	for (int i=0; i<cache->S; ++i)
		for (int j=0; j<cache->E; ++j)
		{
			cache->line[i][j].valid = 0;
			cache->line[i][j].tag = -1;
			cache->line[i][j].time_stamp = 0;
		}
	
	return cache;
}

int hit_or_miss(int op_s, int op_tag)
{
	if (op_s > cache->S)	
		return -2; // 非法输入
	for (int i=0; i<cache->E; ++i)
		if (cache->line[op_s][i].valid && cache->line[op_s][i].tag==op_tag)
			return 1; // hit
	return -1; // miss
}

int is_full(int op_s)
{
	// 判断某组是否已经满
	int full = 1;
	for (int i=0; i<cache->E; ++i)
		if (cache->line[op_s][i].valid == 0)
		{
			full = 0;
			break;
		}
	return full;
}

void update(int i, int op_s, int op_tag)
{
	// 仅在更新cache块时，累计时间戳，因为只需要相对大小关系。
	cache->line[op_s][i].valid = 1;
	cache->line[op_s][i].tag = op_tag;
	for (int j=0; j<cache->E; ++j)
		if (cache->line[op_s][i].valid == 1)
			++(cache->line[op_s][j].time_stamp);	
	cache->line[op_s][i].time_stamp = 0;
}

int LRU_find(int op_s, int op_tag)
{
	// 组 cache 块均满的情况调用
	int max_i = 0;
	int max_time_stamp = -1;
	for (int i=0; i<cache->E; ++i)
	{
		if (cache->line[op_s][i].time_stamp > max_time_stamp)
		{
			max_time_stamp = cache->line[op_s][i].time_stamp;
			max_i = i;	
		}
	}
	return max_i;
}

int main(int argc, const char *argv[])
{
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
