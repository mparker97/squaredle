#ifndef WORD_LIST_H
#define WORD_LIST_H

#include "common.h"

#define WORD_LIST_MIN_SZ 8

struct word_list{
	// a_list fields {
	struct word_data* ls; // heap memory
	int sz;
	int elm_sz;
	// }
	struct{
		int bound; // Cumulative word total by length
			// So, words of length l are [ idx[l - 1].found, idx[l].found )
		int not_found; // idx of first not found word of this length
			// So, found words are [ idx[l - 1].found, idx[l].not_found )
			// and not found words are [ idx[l].not_found, idx[l].found )
	}* idx;
	int idx_sz;
};

struct word_data* word_list_add(struct word_list* wl, const unsigned char* n, unsigned char n_wildcard, unsigned char n_repeat);
#define word_list_combine(x, y) a_list_combine((struct a_list*)(x), (struct a_list*)(y))
void word_list_sort(struct word_list* wl);
void word_list_deinit(struct word_list* wl);
#define word_list_init(x, e) a_list_init((struct a_list*)(x), e)
bool word_list_finalize(struct word_list* wl);
int word_list_binary_search(const struct word_list* wl, const char* word, int len, int* found_pos);
int word_list_has_base(const struct word_list* wl, const char* word, bool mark);
#define word_list_has(wl, word) word_list_has_base(wl, word, false)

#endif
