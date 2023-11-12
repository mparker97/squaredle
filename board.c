#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "board.h"

#define WORD_LIST_MIN_SZ 8

// Free the linked list of tiles 'tl', NOT the tiles themselves.
void tile_list_free(struct tile_list* tl){
	struct tile_list* t;
	while (tl){
		t = tl;
		tl = tl->next;
		free(t);
	}
	// TODO: free tl?
}

char* word_list_add(struct word_list* wl, char* word){
	if (!(wl->sz & ~(wl->sz - 1)) && wl->sz > WORD_LIST_MIN_SZ){
		wl->list = realloc(wl->sz * 2 * sizeof(*wl->list));
		if (wl->list == NULL){
			// TODO: error
			return NULL;
		}
	}
	if (!(wl->list[wl->sz++] = strdup(word))){
		// TODO: error
		return NULL;
	}
	return wl->list[wl->sz - 1];
}

// Append list b to the end of list a
// Accessing b's list after this function is indeterminate
int word_list_combine(struct word_list* a, struct word_list* b){
	a->list = realloc(POW2_ROUNDUP(a->sz + b->sz) * sizeof(*a->list));
	if (a->list == NULL){
		// TODO: realloc failed
		return 0;
	}
	memcpy(&a->list[a->sz], b->list, b->sz * sizeof(*b->list));
	a->sz += b->sz;
	free(b->list);
	b->list = NULL;
	return 1;
}

// Sort by length then alphabetically
int word_list_sort_func(const void* a_, const void* b_){
	char* a, *b;
	int la, lb;
	a = (char*)a_;
	b = (char*)b_;
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
		free(wl->list[i]);
	}
	free(wl->list);
}

int word_list_init(struct word_list* wl){
	wl->list = malloc(WORD_LIST_MIN_SZ * sizeof(*wl->list));
	if (wl->list == NULL){
		// TODO: error
		return 0;
	}
	wl->sz = WORD_LIST_MIN_SZ;
	return 1;
}

// Create the word-length index for f->wl.
// f->wl must be sorted by word_list_sort.
int word_list_finalize(struct word_list_final* f){
	int i, max_len;
	max_len = strlen(f->wl.list[f->wl.sz - 1]);
	f->idx = calloc(max_len, sizeof(struct wlf_idx));
	if (f->idx == NULL){
		// TODO: error
		return 0;
	}
	for (i = 0; i < f->wl.sz; i++){
		f->idx[strlen(f->wl.list[i])].idx++;
	}
	f->idx_sz = max_len;
	return 1;
}

void obfuscate_word(char* buf, const char* word){
	/*
	level 0:
	4		: a***					(1, 0)
	5		: a****					(1, 0)
	6		: ab****				(2, 0)
	7		: ab****g				(2, 1)
	8		: ab****gh			(2, 2)
	9		: ab*****hi			(2, 2)
	10	: ab*****hij		(2, 3)
	11	: ab******ijk		(2, 3)
	12+	: abc******jkl	(3, 3)
	level 1:
	4		: ab**					(2, 0)
	5		: ab***					(2, 0)
	6		: ab***f				(2, 1)
	7		: abc***g				(3, 1)
	8		: abc***gh			(3, 2)
	9		: abc****hi			(3, 2)
	10	: abc****hij		(3, 3)
	11	: abc*****ijk		(3, 3)
	12+	: abcd*****jkl	(4, 3)
	*/
	// TODO: level
	int len, before, after;
	len = strlen(word);
	if (len >= 12){
		before = after = 3;
	}
	else{
		after = len / 2 - 2;
		if (len < 6){
			before = 1;
		}
		else{
			before = 2;
			if (len == 6){
				after == 0;
			}
		}
	}
	memcpy(buf, word, len + 1);
	memset(buf + before, '*', len - before - after);
}

#define GWL_MARKED 1
#define GWL_VISITED 2
#define GWL_START 4
#define GWL_WILDCARD_SHIFT 3

struct gwl_struct{
	struct word_file* wf;
	struct tile* board;
	int len; // length of board
	pthread_mutex_t board_lock;
	pthread_mutex_t idx_lock;
	adjacency_bitmap_t adjacency_bitmap;;
	int processed; // number of prefixes processed [locked]
	int inc;
};

struct gwl_recurse{
	struct tile* board;
	const char* word;
	char* bm;
	int depth;
};

struct gwl_ret{
	struct word_list wl;
	struct word_list bonus;
};

// Recursive function to search for the word in the board
int gwl_mark_words_helper(struct gwl_recurse* gwlr, struct tile* t){
	int ret, i, j;
	ret = 0;
	if (gwlr->word[gwlr->depth] = 0){
		// word found
		ret = 1;
	}
	else{
		gwlr->depth++;
		for (i = 0; i < t->neighbors_len; i++){
			j = (t->neighbors[i] - gwlr->board) / sizeof(struct tile);
			if (t->neighbors[i].letter == WILDCARD){
				if (gwlr->bm[j] >> GWL_WILDCARD_SHIFT == 0){ // TODO: could also check VISITED
					gwlr->bm[i] |= (word[0] - 'A' + 1) << GWL_WILDCARD_SHIFT;
				}
				else if (gwlr->bm[j] >> GWL_WILDCARD_SHIFT != gwlr->word[gwlr->depth - 1] - 'A' + 1){
					continue;
				}
			}
			else if (t->neighbors[i].letter != gwlr->word[gwlr->depth - 1]){
				continue;
			}
			if (!(gwlr->bm[j] & GWL_VISITED) || t->neighbors[i].can_repeat){
				gwlr->bm[j] |= GWL_VISITED;
				if (gwl_mark_words_helper(gwlr, &t->neighbors[i])){
					gwlr->bm[j] |= GWL_MARKED;
					ret = 1;
				}
				gwlr->bm[j] &= ~GWL_VISITED;
			}
		}
		gwlr->depth--;
	}
	return ret;
}

// Search for 'word' in the board, marking all tiles that are part of an instance in a bitmap
// Then, grab the board lock and adjust the freqs according to the bitmap
int gwl_mark_words(struct gwl_struct* s, const char* word){
	struct gwl_recurse gwlr;
	int ret, i;
	gwlr.board = s->board;
	gwlr.word = word;
	gwlr.bm = calloc(s->len, sizeof(char));
	if (gwlr.bm == NULL){
		// TODO: fail
	}
	gwlr.depth = 1;
	ret = 0;
	// Loop through all tiles to start searching for 'word'
	for (i = 0; i < s->len; i++){
		if (s->board[i].letter == WILDCARD){
			gwlr.bm[i] |= (word[0] - 'A' + 1) << GWL_WILDCARD_SHIFT;
		}
		else if (s->board[i].letter != word[0]){
			continue;
		}
		gwlr.bm[i] |= GWL_VISITED;
		if (gwl_mark_words_helper(gwlr, &s->board[i])){
			gwlr.bm[i] |= GWL_MARKED | GWL_START;
			ret = 1;
		}
		gwlr.bm[i] &= ~GWL_VISITED;
	}
	if (ret){
		// Word was found; mark freqs in board
		pthread_mutex_lock(&s->board_lock);
		for (i = 0; i < s->len; i++){
			if (gwlr.bm[i] & GWL_MARKED){
				s->board[i].freq += s->inc;
			}
			if (gwlr.bm[i] & GWL_START){
				s->board[i].start_freq += s->inc;
			}
		}
		pthread_mutex_unlock(&s->board_lock);
	}
	free(bm);
	return ret;
}

// Loop through words in word file that start with the prefix indexed at 'x'
struct word_list* gwl_parse_prefix(struct gwl_struct* s, struct gwl_ret* r, int x){
	char buf[MAX_WORD_LEN + 1];
	unsigned int pos, high;
	int i, j, k;
	char type;
	memcpy(buf, s->wf.idx.key_len, s->wf.idx.ls[x].key);
	buf[s->wf.idx.key_len] = 0;
	// Check that adjacency bitmap allows this prefix
	if (!adjacency_bitmap_all(s->adjacency_bitmap, buf)){
		return r;
	}
	// Get bounds for this prefix
	pos = s->wf.idx.ls[x].value;
	if (x == s->wf.idx.sz - 1){
		high = s->wf.idx.file_pos;
	}
	else{
		high = s->wf.idx.ls[x + 1].value
	}
	// Loop through the words
	// j starts at key_len because prefixes are stripped off in word file
	for (i = pos, j = s->wf.idx.key_len; i < high; i++){
		if (j == wf->max_word_len){
			// TODO: found a word too long
		}
		buf[j] = fgetc(wf->f);
		if (buf[j] <= 1){
			type = buf[j]; // 1 = word, 2 = bonus word // TODO
			buf[j] = 0;
			// End of word; search for it in board then reset back to the prefix
			// First check the adjacency bitmap
			if (!adjacency_bitmap_all(s->adjacency_bitmap, buf)){
				goto not_found;
			}
			if (gwl_mark_words(s, buf)){
				if (type == 0){
					if (word_list_add(&r->wl, buf) == NULL){
						// TODO: error
					}
				}
				else{ // type == 1
					if (word_list_add(&r->bonus, buf) == NULL){
						// TODO: error
					}
				}
			}
not_found:
			j = s->wf.idx.key_len;
		}
		else{
			j++;
		}
	}
	return r; // TODO: not needed but we might want to use return for errors
}

// Thread function that grabs the next prefix and processes words beginning with it
// Returned word_list must be freed
static struct gwl_ret* gwl_thd_func(void* ptr){
	struct gwl_ret* ret;
	FILE* f;
	struct gwl_struct* s;
	int x; // The prefix index
	s = (struct gwl_struct*)ptr;
	f = fopen(s->wf->name, "r");
	if (f == NULL){
		// TODO: fail
	}
	ret = malloc(sizeof(struct gwl_ret));
	if (ret == NULL){
		// TODO: fail
	}
	if (!word_list_init(&ret->wl)){
		// TODO: fail
	}
	if (!word_list_init(&ret->bonus)){
		// TODO: fail
	}
	for (;;){
		// lock the index to obtain the next prefix
		pthread_mutex_lock(&s->idx_lock);
		if (s->processed < s->wf.idx.sz){
			x = s->processed++;
		}
		else{
			break;
		}
		pthread_mutex_unlock(&s->idx_lock);
		// Append list of relevant words that start with this prefix
		gwl_parse_prefix(s, ret, x);
	}
	// loop broke when thread realized there are no more prefixes left, so it is holding the lock
	pthread_mutex_unlock(&s->idx_lock);
	fclose(f);
	return ret;
}

#define NUM_THDS 4

// Generate the sorted word list for the given tile list 'board' of length 'len' and word file 'wf'
// This uses multithreading to find all words and mark the frequencies of each tile
int get_word_list(struct tile* board, int len, struct word_file* wf, struct word_list_final* f){
	pthread_t thds[NUM_THDS];
	struct gwl_ret* r; // heap
	struct gwl_struct s;
	int i;
	s.wf = wf;
	s.board = board;
	s.len = len;
	pthread_mutex_init(&s.board_lock, NULL);
	pthread_mutex_init(&s.idx_lock, NULL);
	s.adjacency_bitmap = bm; // TODO
	word_list_init(&f->wl);
	word_list_init(&f->bonus);
	s.processed = 0;
	s.inc = 1;
	for (i = 0; i < NUM_THDS; i++){
		if (pthread_create(&thds[i], NULL, gwl_thd_func, &s) != 0){
			// TODO: error
			return 0;
		}
	}
	for (i = 0; i < NUM_THDS; i++){
		pthread_join(thds[i], &r);
		if (r == NULL){
			// TODO: fail
			return 0;
		}
		if (!word_list_combine(&f->wl, &r->wl)){
			// TODO: fail
			return 0;
		}
		if (!word_list_combine(&f->bonus, &r->bonus)){
			// TODO: fail
			return 0;
		}
		free(r);
	}
	word_list_sort(&f->wl);
	word_list_sort(&f->bonus);
	word_list_finalize(f);

	pthread_mutex_destroy(&s.idx_lock);
	pthread_mutex_destroy(&s.board_lock);
	return 1;
}

void get_adjacency_bitmap(struct tile* board, int len, adjacency_bitmap_t bm){
	int i, j;
	memset(bm, 0, ADJACENCY_BM_SZ * sizeof(unsigned int));
	for (i = 0; i < len; i++){
		for (j = 0; j < board[i].neighbors_len; j++){
			if (adjacency_bitmap_set(bm, board[i].letter, board[i].neighbors[j].letter)){
				// Adjacent wildcards wash out bitmap
				return;
			}
		}
	}
} 

void send_word(struct tile_list* tl, struct tile* board, int len){
	struct gwl_struct gwls;
	char buffer[TODO_LEN];
	struct tile_list* next;
	int i;
	for (next = tl, i = 0; next != NULL; next = next->next, i++){
		buffer[i] = next->letter;
	}
	buffer[i] = 0;
	if (word_list_has(wl, buffer)){
		// Word is valid, decrement freqs
		gwls.board = board;
		gwls.len = len;
		pthread_mutex_init(&gwls.board_lock);
		gwls.inc = -1;
		gwl_mark_words(&gwls, buffer);
		pthread_mutex_destroy(&gwls.board_lock);
	}
	tile_list_free(tl);
}

