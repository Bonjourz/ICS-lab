/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
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
	if (N == 32) {
		/* Divide the matrix of 32x32 into blocks of 8x8
		 * and then manage it
		 */
		int i, j, k, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
		for (i = 0; i < 32; i += 8) {
			for (j = 0; j < 32; j += 8) {
				for (k = 0; k < 8; k++) {
					tmp0 = A[i+k][j];
					tmp1 = A[i+k][j+1];
					tmp2 = A[i+k][j+2];
					tmp3 = A[i+k][j+3];
					tmp4 = A[i+k][j+4];
					tmp5 = A[i+k][j+5];
					tmp6 = A[i+k][j+6];
					tmp7 = A[i+k][j+7];
					B[j][i+k] = tmp0;
					B[j+1][i+k] = tmp1;
					B[j+2][i+k] = tmp2;
					B[j+3][i+k] = tmp3;
					B[j+4][i+k] = tmp4;
					B[j+5][i+k] = tmp5;
					B[j+6][i+k] = tmp6;
					B[j+7][i+k] = tmp7;
				}
			}
		}
	}
	else if (N == 67) {
		int i, j, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
		for (j = 0; j < 56; j += 8) {
			// At first manage 56 columns
			for (i = 0; i < 67; i++) {
				tmp0 = A[i][j];
				tmp1 = A[i][j+1];
				tmp2 = A[i][j+2];
				tmp3 = A[i][j+3];
				tmp4 = A[i][j+4];
				tmp5 = A[i][j+5];
				tmp6 = A[i][j+6];
				tmp7 = A[i][j+7];
				B[j][i] = tmp0;
				B[j+1][i] = tmp1;		
				B[j+2][i] = tmp2;		
				B[j+3][i] = tmp3;		
				B[j+4][i] = tmp4;		
				B[j+5][i] = tmp5;		
				B[j+6][i] = tmp6;		
				B[j+7][i] = tmp7;
			}
		}
		for (i = 0; i < 67; i++) {
			//Manage the last five columns
			tmp0 = A[i][56];
			tmp1 = A[i][57];
			tmp2 = A[i][58];
			tmp3 = A[i][59];
			tmp4 = A[i][60];
			B[56][i] = tmp0;
			B[57][i] = tmp1;
			B[58][i] = tmp2;
			B[59][i] = tmp3;
			B[60][i] = tmp4;
		}
	}

	else if (N == 64) {
		/* A11  A12		A11	 A21
		 *			-->
		 * A21  A22		A12  A22
		 */
		int i, j, k, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
		for (j = 0; j < 64; j += 8) {
			for (i = 0; i < 64; i += 4) {
				if (i % 8 == 0) {
					/* At first change change the B into:
					 *
					 * A11   A12
					 *
					 * ***	 ***
					 *
					 */
					for (k = 0; k < 4; k++) {
						tmp0 = A[i+k][j];
						tmp1 = A[i+k][j+1];
						tmp2 = A[i+k][j+2];
						tmp3 = A[i+k][j+3];
						tmp4 = A[i+k][j+4];
						tmp5 = A[i+k][j+5];
						tmp6 = A[i+k][j+6];
						tmp7 = A[i+k][j+7];
						
						B[j][i+k] = tmp0;
						B[j+1][i+k] = tmp1;
						B[j+2][i+k] = tmp2;
						B[j+3][i+k] = tmp3;

						B[j][i+k+4] = tmp4;
						B[j+1][i+k+4] = tmp5;
						B[j+2][i+k+4] = tmp6;
						B[j+3][i+k+4] = tmp7;
					}
				}
				else {
					for (k = 0; k < 4; k++) {
						/* Transfer the B into:
						 *
						 *  A11   A21
						 *
						 *  A12   ***
						 *
						 */
						tmp0 = B[j+k][i];
						tmp1 = B[j+k][i+1];
						tmp2 = B[j+k][i+2];
						tmp3 = B[j+k][i+3];
						
						tmp4 = A[i][j+k];
						tmp5 = A[i+1][j+k];
						tmp6 = A[i+2][j+k];
						tmp7 = A[i+3][j+k];

						B[j+k][i] = tmp4;
						B[j+k][i+1] = tmp5;
						B[j+k][i+2] = tmp6;
						B[j+k][i+3] = tmp7;

						B[j+k+4][i-4] = tmp0;
						B[j+k+4][i-3] = tmp1;
						B[j+k+4][i-2] = tmp2;
						B[j+k+4][i-1] = tmp3;
					}
					for (k = 0; k < 4; k++) {
						/* change B into:
						 *
						 * A11  A21
						 *
						 * A12  A22
						 *
						 */
						tmp0 = A[i+k][j+4];
						tmp1 = A[i+k][j+5];
						tmp2 = A[i+k][j+6];
						tmp3 = A[i+k][j+7];

						B[j+4][i+k] = tmp0;
						B[j+5][i+k] = tmp1;
						B[j+6][i+k] = tmp2;
						B[j+7][i+k] = tmp3;
					}
				}
			}
		}
	}

	else {
		int i, j;
		for (i = 0; i < N; i++) 
			for (j = 0; j < M; j++)
				A[i][j] = B[j][i];
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

