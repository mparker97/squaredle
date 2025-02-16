#ifndef BOARD_H
#define BOARD_H
#include "common.h"

struct tile{
	struct tile** neighbors;
	int start_freq;
	int freq;
	unsigned char letter;
	char can_repeat;
	char neighors_len;
	// TODO: Still need:
	// temporarily assigned letter for wildcards
	// some type of ref count
		// mark if visited during tile_list building (or number of times visited if can_repeat is true)
		
};

struct score{
	int wl_found;
	int bonus_found;
	int score;
	int bonus_score;
	int score_total;
};

#define WORD_FOUND 0
#define WORD_NOT_FOUND 1
#define WORD_ALREADY_FOUND 2
#define WORD_TOO_SHORT 3
#define WORD_BONUS (1 << 2)

struct word_data{
	const unsigned char* word;
	unsigned char len; // TODO: needed?
	unsigned char n_repeat; // Minimum number of can_repeat letters
	unsigned char n_wildcard; // Minimum number of wildcard letters
};

#define ADJACENCY_BM_SZ 26
typedef unsigned int[ADJACENCY_BM_SZ] adjacency_bm_t;

int adjacency_bitmap_set(adjacency_bm_t bm, unsigned char x, unsigned char y){
	unsigned char c;
	if (x == '?' && y == '?'){
		memset(bm, -1, ADJACENCY_BM_SZ * sizeof(unsigned int));
		return 1;
	}
	else if (x == '?'){
		for (c = 'A'; c <= 'Z'; c++){
			bm[c] |= 1 << (y - 'A');
		}
	}
	else if (y == '?'){
		for (c = 'A'; c <= 'Z'; c++){
			bm[x - 'A'] |= 1 << (c);
		}
	}
	else {
		bm[x - 'A'] |= 1 << (y - 'A');
	}
	return 0;
}

inline int adjacency_bitmap_get(const adjacency_bm_t bm, unsigned char x, unsigned char y){
	return bm[x - 'A'] & (1 << (y - 'A'));
}

// 'buf' must be a string of length >= 1
int adjacency_bitmap_all(const adjacency_bm_t bm, const unsigned char* buf){
	while (buf[1]){
		if (!adjacency_bitmap_get(bm, buf[0], buf[1])){
			return 0;
		}
		buf++;
	}
	return 1;
}

void obfuscate_word(unsigned char* buf, const struct word_data* wd);
int send_word(const char* buf, struct tile* board, int len);

#endif
