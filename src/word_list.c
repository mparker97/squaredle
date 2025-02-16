#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "board.h"
#include "common.h"
#include "word_list.h"

struct word_data* word_list_add(struct word_list* wl, const unsigned char* n, unsigned char n_wildcard, unsigned char n_repeat){
	struct word_data* ret;
	unsigned char* z;
	z = strdup(n);
	if (z == NULL){
		// TODO: error
		return NULL;
	}
	ret = a_list_add(wl);
	if (ret == NULL){
		free(z);
		// TODO: error
		return NULL;
	}
	ret->word = z;
	ret->n_wildcard = n_wildcard;
	ret->n_repeat = n_repeat;
	return ret;
} 

// Sort by length then alphabetically
int word_list_sort_func(const void* a_, const void* b_){
	unsigned char* a, *b;
	int la, lb;
	a = (unsigned char*)a_;
	b = (unsigned char*)b_;
	la = strlen(a);
	lb = strlen(b);
	if (la != lb){
		return la - lb;
	}
	return strcmp(a, b);
}

void word_list_sort(struct word_list* wl){
	qsort(wl->list, wl->sz, sizeof(*wl->list), (int(*)(const void*, const void*))&strcmp);
}

void word_list_deinit(struct word_list* wl){
	int i;
	for (i = 0; i < wl->sz; i++){
		free(wl->list[i].word);
	}
	free(wl->list);
}

// Create the word-length index for wl.
// wl must be sorted by word_list_sort.
bool word_list_finalize(struct word_list* wl){
	int i, max_len;
	max_len = strlen(wl->list[wl->sz - 1]);
	wl->idx = calloc(max_len + 1, 2 * sizeof(int));
	if (wl->idx == NULL){
		// TODO: error
		return false;
	}
	// Count number of words for each length
	for (i = 0; i < wl->sz; i++){
		wl->idx[strlen(wl->list[i]) + 1].bound++;
	}
	// Accumulate counts
	for (i = 1; i < max_len; i++){
		wl->idx[i].bound += wl->idx[i - 1].bound;
		wl->idx[i].not_found = wl->idx[i - 1].bound;
	}
	wl->idx_sz = max_len;
	return true;
}

int word_list_binary_search(const struct word_list* wl, const char* word, int len, int* found_pos){
	int low, high, mid, x;
	bool found;
	found = true;
	low = wl->idx[len - 1].bound;
	high = wl->idx[len].not_found;
	for (;;){
		if (low >= high){
			if (found){
				// Not found found words, search unfound words
				found = false;
				if (found_pos){
					*found_pos = high;
				}
				low = wl->idx[len].not_found;
				high = wl->idx[len].bound;
			}
			else{
				// Not found at all
				return -1;
			}
		}
		mid = (high - low) / 2 + low;
		x = strcmp(word, wl->ls[mid].word);
		if (x == 0){
			// Found
			return mid;
		}
		else if (x < 0){
			high = mid;
		}
		else{
			low = mid + 1;
		}
	}
}

int word_list_has_base(const struct word_list* wl, const char* word, bool mark){
	// TODO: give negative return values meaning
	struct word_data wd;
	size_t l;
	int x, y, i;
	l = strlen(word);
	if (l < MIN_WORD_LEN){
		return -2; // Too short
	}
	if (wl->idx_sz < l){
		return -1; // Not found
	}
	x = word_list_binary_search(wl, word, l, &y);
	if (x < 0){
		return -1; // Not found
	}
	if (x < wl->idx[l].not_found){
		return -3 // Already found
	}
	if (mark){
		// Mark this word as found; moving it into the found list
		memcpy(&wd, &wl->ls[x], sizeof(struct word_data));
		memmove(&wl->ls[y + 1], &wl->ls[y], (x - 1 - y) * sizeof(struct word_data));
		memcpy(&wl->ls[y], &wd, sizeof(struct word_data));
		wl->idx[l].not_found++;
		return y;
	}
	return x;
}

