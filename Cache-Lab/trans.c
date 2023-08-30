// Author: Aron
// Date: 4/25/2023

/* Requirements:
 1. At most 12 local variables of type int per transpose function.
 2. You are not allowed to side-step the previous rule by using any variables of type long or by using
   any bit tricks to store more than one value to a single variable.
 3. May not use recursion.
 4. If you choose to use helper functions, you may not have more than 12 local variables on the stack
   at a time between your helper functions and your top level transpose function. For example, if your
   transpose declares 8 variables, and then you call a function which uses 4 variables, which calls another
   function which uses 2, you will have 14 variables on the stack, and you will be in violation of the rule
 5. Your transpose function may not modify array A. You may, however, do whatever you want with the
    contents of array B.
 6. You are NOT allowed to define any arrays in your code or to use any variant of malloc.
 */

/* Suggestions & hints:
 1. Your code only needs to be correct for these three cases and you can optimize it specifically for 
    these three cases.
 2. In particular, it is perfectly OK for your function to explicitly check for the input sizes and 
    implement separate code optimized for each case.
 3. It then evaluates each trace by running the reference simulator on a cache with parameters
    (s = 5, E = 1, b = 5).
 */

/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes. ( s=5, b=5)
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{

    // 32 x 32
    if (M==32 && N==32)
    {
        // 考虑 8 x 8 分块
        for (int m=0; m<32; m+=8)
            for (int n=0; n<32; n+=8)
                for (int i=m; i<m+8; ++i)
                {
                    int a0 = A[i][n];
                    int a1 = A[i][n+1];
                    int a2 = A[i][n+2];
                    int a3 = A[i][n+3];
                    int a4 = A[i][n+4];
                    int a5 = A[i][n+5];
                    int a6 = A[i][n+6];
                    int a7 = A[i][n+7];
                    B[n][i] = a0;
                    B[n+1][i] = a1;
                    B[n+2][i] = a2;
                    B[n+3][i] = a3;
                    B[n+4][i] = a4;
                    B[n+5][i] = a5;
                    B[n+6][i] = a6;
                    B[n+7][i] = a7;
                }
    }
    // 64 x 64
    else if (M==64 && N==64)
    {
        // 考虑 2 x 4 x 8 分块 : 写入大于4时会导致驱逐，1个cache行使用的数据小于8时导致浪费
        int a0, a1, a2, a3, a4, a5, a6, a7;
        for (int m=0; m<64; m+=8)
            for (int n=0; n<64; n+=8)
            {
                // 8x8 内部化为 4x4 矩阵操作
                for (int i=m; i<m+4; ++i)
                {
                    a0 = A[i][n];
                    a1 = A[i][n+1];
                    a2 = A[i][n+2];
                    a3 = A[i][n+3];
                    a4 = A[i][n+4];
                    a5 = A[i][n+5];
                    a6 = A[i][n+6];
                    a7 = A[i][n+7];
                    
                    B[n][i] = a0;
                    B[n+1][i] = a1;
                    B[n+2][i] = a2;
                    B[n+3][i] = a3;
                    B[n][i+4] = a4;
                    B[n+1][i+4] = a5;
                    B[n+2][i+4] = a6;
                    B[n+3][i+4] = a7;
                }
                // 副对角线 2个 4x4 矩阵对调位置
                for (int j=n; j<n+4; ++j)
                {
                    // 存储 B的右上 4x4 矩阵 （此时还在cache中） ： 按行
                    a0 = B[j][m+4];
                    a1 = B[j][m+5];
                    a2 = B[j][m+6];
                    a3 = B[j][m+7];
                    
                    // 读取 A的左下 4x4 矩阵 （此时会驱逐B的对应cache行) : 按列
                    a4 = A[4+m][j];
                    a5 = A[5+m][j];
                    a6 = A[6+m][j];
                    a7 = A[7+m][j];
                    
                    // A左下移动到B右上
                    B[j][m+4] = a4;
                    B[j][m+5] = a5;
                    B[j][m+6] = a6;
                    B[j][m+7] = a7;
                    
                    // B右上移动到B左下 
                    B[4+j][m] = a0;
                    B[4+j][m+1] = a1;
                    B[4+j][m+2] = a2;
                    B[4+j][m+3] = a3; 
                }
                // B右下块处理
                for (int k=m+4; k<m+8; ++k)
                {
                    a0 = A[k][n+4];
                    a1 = A[k][n+5];
                    a2 = A[k][n+6];
                    a3 = A[k][n+7];

                    B[n+4][k] = a0;
                    B[n+5][k] = a1;
                    B[n+6][k] = a2;
                    B[n+7][k] = a3;
                }
            }
    }
    // 61 x 67
    else if (M==61 && N==67)
    {
        for (int i = 0; i < N; i += 16)
            for (int j = 0; j < M; j += 16)
                for (int k = i; k < i + 16 && k < N; k++)
                    for (int s = j; s < j + 16 && s < M; s++)
                        B[s][k] = A[k][s];
    }
}
/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

