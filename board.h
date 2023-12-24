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
	char visited; // ref count
};

struct tile_list{ // linked list
	struct tile* tile;
	struct tile_list* next;
};

struct word_data{
	const unsigned char* word;
	unsigned char len; // TODO: needed?
	char found;
	unsigned char n_repeat; // Minimum number of can_repeat letters
	unsigned char n_wildcard; // Minimum number of wildcard letters
};

struct word_list{ // ptr_a_list
	struct word_data* ls; // heap memory
	int sz;
	int elm_sz;
	int* idx; // Cumulative word total by length
	int idx_sz;
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

#endif
