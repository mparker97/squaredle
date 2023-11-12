#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SZ_R 10
#define SZ_C 10
#define MIN_LEN 4
#define MAX_LEN 4 // (SZ_R * SZ_C)

unsigned int count_neighbors(char board[SZ_R][SZ_C], int r, int c, int depth){
	unsigned int ret;
	int i, j;
	ret = 0;
	if (depth < MAX_LEN){
		for (i = -1; i <= 1; i++){
			for (j = -1; j <= 1; j++){
				if (r + i >= 0 && r + i < SZ_R && c + j >= 0 && c + j < SZ_C){
					if (board[r + i][c + j] == 0){
						board[r + i][c + j] = 1;
						if (depth + 1 >= MIN_LEN){
							ret++;
						}
						ret += count_neighbors(board, r + i, c + j, depth + 1);
						board[r + i][c + j] = 0;
					}
				}
			}
		}
	}
	return ret;
}

int main(int argc, char* argv[]){
	unsigned int counts[SZ_R][SZ_C];
	char board[SZ_R][SZ_C];
	unsigned int total;
	int i, j;
	total = 0;
	for (i = 0; i < SZ_R; i++){
		for (j = 0; j < SZ_C; j++){
			memset(board, 0, sizeof(board));
			board[i][j] = 1;
			if (MIN_LEN == 1){
				counts[i][j] = 1;
			}
			else{
				counts[i][j] = 0;
			}
			counts[i][j] += count_neighbors(board, i, j, 1);
			total += counts[i][j];
			printf("%u ", counts[i][j]);
		}
		printf("\n");
	}
	printf("total: %u\n", total);
	return 0;
}
