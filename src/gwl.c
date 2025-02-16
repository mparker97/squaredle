#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "board.h"
#include "common.h"
#include "gwl.h"

#define GWL_MARKED 1
#define GWL_VISITED 2
#define GWL_START 4
#define GWL_WILDCARD_SHIFT 3

struct gwl_mark{
	struct tile* board;
	pthread_mutex_t board_lock;
	int len; // length of board
	int inc; // How much to increment each count (+1 or -1)
	unsigned char n_wildcard;
	unsigned char n_repeat;
};

struct gwl_struct{
	struct word_file* wf;
	pthread_mutex_t idx_lock;
	adjacency_bitmap_t adjacency_bitmap;
	struct gwl_mark mark;
	int processed; // number of prefixes processed [@idx_lock]
};

struct gwl_recurse{
	struct tile* board;
	const unsigned char* word;
	unsigned char* bm;
	int depth;
	// # of wildcards encountered and low watermark
	unsigned char wildcard_ref;
	unsigned char n_wildcard;
	// # of can_repeats encountered and low watermark
	unsigned char repeat_ref;
	unsigned char n_repeat;
};

struct gwl_ret{
	struct word_list wl;
	struct word_list bonus;
};

bool gwl_mark_words_helper(struct gwl_recurse* gwlr, struct tile* t);

bool gwl_mark_words_helper_tile(struct gwl_recurse* gwlr, struct tile* n){
	bool ret;
	int j;
	int wildcard_flag, repeat_flag;
	wildcard_flag = repeat_flag = 0;
	j = (n - gwlr->board) / sizeof(struct tile);
	if (n->letter == WILDCARD){
		if (gwlr->bm[j] >> GWL_WILDCARD_SHIFT == 0){
			// Wildcard not yet used; set it to this letter
			gwlr->bm[j] |= (word[0] - 'A' + 1) << GWL_WILDCARD_SHIFT;
		}
		else if (gwlr->bm[j] >> GWL_WILDCARD_SHIFT != gwlr->word[gwlr->depth - 1] - 'A' + 1){
			// Wildcard used as a different letter; dead end
			return false;
		}
		wildcard_flag = 1;
	}
	else if (n->letter != gwlr->word[gwlr->depth - 1]){
		// Wrong letter; dead end
		return false;
	}
	if (n->can_repeat){
		repeat_flag = 1;
	}
	else if (gwlr->bm[j] & GWL_VISITED){
		// Already visited; dead end
		return false;
	}
	gwlr->bm[j] |= GWL_VISITED;
	gwlr->wildcard_ref += wildcard_flag;
	gwlr->repeat_ref += repeat_flag;
	if (ret = gwl_mark_words_helper(gwlr, n)){
		gwlr->bm[j] |= GWL_MARKED;
	}
	gwlr->repeat_ref -= repeat_flag;
	gwlr->wildcard_ref -= wildcard_flag;
	gwlr->bm[j] &= ~GWL_VISITED;
	return ret;
}

// Recursive function to search for the word in the board
bool gwl_mark_words_helper(struct gwl_recurse* gwlr, struct tile* t){
	bool ret;
	int i;
	ret = false;
	if (gwlr->word[gwlr->depth] = 0){
		// word found
		min_update(gwlr->n_wildcard, gwlr->wildcard_ref);
		min_update(gwlr->n_repeat, gwlr->repeat_ref);
		ret = true;
	}
	else {
		gwlr->depth++;
		for (i = 0; i < t->neighbors_len; i++){
			ret |= gwl_mark_words_helper_tile(gwlr, &t->neighbors[i])
		}
		gwlr->depth--;
	}
	return ret;
}

// Search for 'word' in the board, marking all tiles that are part of an instance in a bitmap
// Then, grab the board lock and adjust the freqs according to the bitmap
bool gwl_mark_words(struct gwl_mark* m, const unsigned char* word){ 
	struct gwl_recurse gwlr;
	bool ret;
	int i;
	gwlr.board = m->board;
	gwlr.word = word;
	gwlr.bm = calloc(m->len, sizeof(unsigned char));
	if (gwlr.bm == NULL){
		// TODO: fail
	}
	gwlr.depth = 1;
	gwlr.wildcard_ref = 0;
	gwlr.n_wildcard = -1;
	gwlr.repeat_ref = 0;
	gwlr.n_repeat = -1;
	ret = false;
	// Loop through all tiles to start searching for 'word'
	for (i = 0; i < m->len; i++){
		if (gwl_recurse_helper_tile(&gwlr, &m->board[i])){
			gwlr.bm[i] |= GWL_START;
			ret = true;
		}
	}
	if (ret){
		// Word was found; mark freqs in board
		// These mark ALL paths that make the word
		// TODO: don't do this with bonus words
		pthread_mutex_lock(&m->board_lock);
		for (i = 0; i < m->len; i++){
			if (gwlr.bm[i] & GWL_MARKED){
				m->board[i].freq += m->inc;
			}
			if (gwlr.bm[i] & GWL_START){
				m->board[i].start_freq += m->inc;
			}
		}
		pthread_mutex_unlock(&m->board_lock);
	}
	free(bm);
	m->n_wildcard = gwlr.n_wildcard;
	m->n_repeat = gwlr.n_repeat;
	return ret;
}

// Loop through words in word file that start with the prefix indexed at 'x'
int gwl_parse_prefix(struct gwl_struct* s, struct gwl_ret* r, int x){
	word_buffer_t buf;
	unsigned int pos, high;
	int i, j, k;
	char type;
	memcpy(buf, s->wf.idx.key_len, s->wf.idx.ls[x].key);
	buf[s->wf.idx.key_len] = 0;
	// Check that adjacency bitmap allows this prefix
	if (!adjacency_bitmap_all(s->adjacency_bitmap, buf)){
		return 1;
	}
	// Get bounds for this prefix
	pos = s->wf.idx.ls[x].value;
	if (x == s->wf.idx.sz - 1){
		high = s->wf.idx.file_pos;
	}
	else {
		high = s->wf.idx.ls[x + 1].value
	}
	fseek(s->wf, pos, SEEK_SET);
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
			if (adjacency_bitmap_all(s->adjacency_bitmap, buf)){
				// Now search the board
				if (gwl_mark_words(&s->mark, buf)){
					if (type == 0){
						if (word_list_add(&r->wl, buf, s->mark.n_wildcard, s->mark.n_repeat) == NULL){
							// TODO: error
						}
					}
					else { // type == 1
						if (word_list_add(&r->bonus, buf, s->mark.n_wildcard, s->mark.n_repeat) == NULL){
							// TODO: error
						}
					}
				}
			}
			j = fgetc(s->wf) + s->wf.idx.key_len;
			i++;
		}
		else {
			j++;
		}
	}
	return 0; // TODO: not needed but we might want to use return for errors
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
	if (!word_list_init(&ret->wl, sizeof(struct word_data))){
		// TODO: fail
	}
	if (!word_list_init(&ret->bonus, sizeof(struct word_data))){
		// TODO: fail
	}
	for (;;){
		// lock the index to obtain the next prefix
		pthread_mutex_lock(&s->idx_lock);
		if (s->processed < s->wf.idx.sz){
			x = s->processed++;
		}
		else {
			pthread_mutex_unlock(&s->idx_lock);
			break;
		}
		pthread_mutex_unlock(&s->idx_lock);
		// Append list of relevant words that start with this prefix
		gwl_parse_prefix(s, ret, x);
	}
	fclose(f);
	return ret;
}

#define NUM_THDS 4

// Generate the sorted word list for the given tile list 'board' of length 'len' and word file 'wf'
// This uses multithreading to find all words and mark the frequencies of each tile
bool get_word_list(struct tile* board, int len, struct word_file* wf, struct word_list* wl, struct word_list* bonus){
	pthread_t thds[NUM_THDS];
	struct gwl_ret* r; // heap
	struct gwl_struct s;
	int i;
	s.wf = wf;
	s.mark.board = board;
	s.mark.len = len;
	pthread_mutex_init(&s.mark.board_lock, NULL);
	pthread_mutex_init(&s.idx_lock, NULL);
	s.adjacency_bitmap = bm; // TODO
	word_list_init(wl, sizeof(struct word_data));
	word_list_init(bonus, sizeof(struct word_data));
	s.processed = 0;
	s.mark.inc = 1;
	for (i = 0; i < NUM_THDS; i++){
		if (pthread_create(&thds[i], NULL, gwl_thd_func, &s) != 0){
			// TODO: error
			return false;
		}
	}
	for (i = 0; i < NUM_THDS; i++){
		pthread_join(thds[i], &r);
		if (r == NULL){
			// TODO: fail
			return false;
		}
		if (!word_list_combine(wl, &r->wl)){
			// TODO: fail
			return false;
		}
		if (!word_list_combine(bonus, &r->bonus)){
			// TODO: fail
			return false;
		}
		free(r);
	}
	word_list_sort(wl);
	word_list_finalize(wl);
	word_list_sort(bonus);
	word_list_finalize(bonus);

	pthread_mutex_destroy(&s.idx_lock);
	pthread_mutex_destroy(&s.mark.board_lock);
	return true;
}

