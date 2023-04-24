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
#include <stdio.h>

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
static char t[100]; // 读入文件名
static int verbose = 0;

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
	for (int i=0; i<cache->S; ++i) // 初始化
		for (int j=0; j<cache->E; ++j)
		{
			cache->line[i][j].valid = 0;
			cache->line[i][j].tag = -1;
			cache->line[i][j].time_stamp = 0;
		}
	return cache;
}

void free_Cache()
{
	int S = cache->S;
	for (int i=0; i<S; ++i)
		free(cache->line[i]);
	free(cache->line);
	free(cache);
}

int hit_or_miss(int op_s, int op_tag)
{
	// 判断 cache 命中还是未命中
	if (op_s > cache->S)
		return -2; // 非法输入
	for (int i=0; i<cache->E; ++i)
		if (cache->line[op_s][i].valid && cache->line[op_s][i].tag==op_tag)
			return 1; // hit
	return -1; // miss
}

int is_full(int op_s)
{
	// 判断某组是否已经满,未满返回索引值
	int index = -1;
	for (int i=0; i<cache->E; ++i)
		if (cache->line[op_s][i].valid == 0)
		{
			index = i;
			break;
		}
	return index;
}

void update_cache(int i, int op_s, int op_tag)
{
	cache->line[op_s][i].valid = 1;
	cache->line[op_s][i].tag = op_tag;
	for (int j=0; j<cache->E; ++j)
		if (cache->line[op_s][i].valid == 1)
			++(cache->line[op_s][j].time_stamp);	
	cache->line[op_s][i].time_stamp = 0;
}

int LRU_find(int op_s)
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

void lookup_cache(int op_s, int op_tag)
{
	if (op_s > cache->S)  return ;
	int index = hit_or_miss(op_s, op_tag);
	if (index == -1)  // miss
	{
		++miss_count;
		if (verbose)
			printf("miss ");
		int i = is_full(op_s);
		if (i == -1)
		{
			++eviction_count;
			if (verbose) printf("eviction");
			i = LRU_find(op_s);
		}
		update_cache(i, op_s, op_tag);
	}
	else  // hit
	{
		++hit_count;
		if (verbose)
			printf("hit ");
		update_cache(index, op_s, op_tag);
	}
}

unsigned int get_op_s(unsigned int addr, int s, int b)
{
	// cache 的组号
	unsigned int op_s = ( addr << (64-s-b) ) >> (64-s);
	return op_s;
}

unsigned int get_op_tag(unsigned int addr, int s, int b)
{
	unsigned int op_tag = addr >> (s+b);
	return op_tag;
}

void get_trace(int s, int E, int b)
{
	// 对输入的数据进行解析
	FILE *pf = fopen(t, "r");
	if (pf == 0)
		exit(-1);
	char op;
	unsigned int address;
	int size;
	// 读入相关变量
	while ((fscanf(pf, "%c %u, %d", &op, &address, &size)) != EOF)
	{
		int op_s = get_op_s(address, s, b);
		int op_tag = get_op_tag(address, s, b);	
		switch(op)
		{
			case 'L':  // 加载指令
				lookup_cache(op_s, op_tag);
				break;
			case 'S':
				lookup_cache(op_s, op_tag);
				break;
			case 'M':
				lookup_cache(op_s, op_tag);
				lookup_cache(op_s, op_tag);
				break;
			default:
				break;
		}
	}
	fclose(pf);
}

void print_help()
{
    printf("** A Cache Simulator by Aron\n");
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("-h         Print this help message.\n");
    printf("-v         Optional verbose flag.\n");
    printf("-s <num>   Number of set index bits.\n");
    printf("-E <num>   Number of lines per set.\n");
    printf("-b <num>   Number of block offset bits.\n");
    printf("-t <file>  Trace file.\n\n\n");
    printf("Examples:\n");
    printf("linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

int main(int argc, char *argv[])
{
	// 解析命令行参数
	char opt;
	int s, E, b;
	while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1)
	{
		switch (opt)
		{
			case 'h':
				print_help(); // 打印用例信息
				exit(0);
			case 'v':
				verbose = 1;  // 显示详细信息
				break;
			case 's':
				s = atoi(optarg);
				break;
			case 'E':
				E = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
				break;
			case 't':
				strcpy(t, optarg);
				break;
			default:
				print_help();
				exit(-1);
		}
	}
	init_cache(s, E, b);
	get_trace(s, E, b);
	free_Cache();
	printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
