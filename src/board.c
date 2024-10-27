#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "board.h"
#include "common.h"

#define WORD_LIST_MIN_SZ 8

extern struct score global_score;

const unsigned char LENGTH_SCORES[] = {
	0, 1, 2, 3, 5, 8, 12, 17, 23, 30, 38, 47, 57, 68, 80, 92, 105, 118, 132, 146, 161, 175, 189, 203, 217, 230, 243,
	0, 12, 24, 35, 46, 56, 66, 75, 84, 92, 100, 107, 114, 120, 126, 131, 136, 140, 144 // (+256)
	// ... (+265 + 3 * (n-45))
};

void score_init(struct score* sc){
	sc.wl_found = 0;
	sc.bonus_found = 0;
	sc.score = 0;
	sc.bonus_score = 0;
	sc.score_total = 0;
}

int word_score(const char* word){
  int n;
	int ret;
	n = strlen(word);
	if (n > 45){
		ret = LENGTH_SCORES[45] + 256 + 3 * (n - 45);
	}
	else{
		ret = LENGTH_SCORES[n];
		if (n > 26){
			ret += 256;
		}
	}
	return ret;
}

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

#define word_list_combine(x, y) a_list_combine((struct a_list*)(x), (struct a_list*)(y))

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

#define word_list_init(x, e) a_list_init((struct a_list*)(x), e)

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

const static char OBFUSCATE_LENS[5][14] = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x11, 0x11, 0x21, 0x21, 0x22, 0x22},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x11, 0x21, 0x21, 0x22, 0x22, 0x22, 0x32},
	{0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x20, 0x21, 0x22, 0x22, 0x23, 0x23, 0x33, 0x33},
	{0x00, 0x00, 0x00, 0x10, 0x10, 0x20, 0x21, 0x22, 0x32, 0x32, 0x33, 0x33, 0x43, 0x43},
	{0x00, 0x00, 0x00, 0x10, 0x20, 0x11, 0x21, 0x32, 0x32, 0x33, 0x33, 0x43, 0x43, 0x44}
}

void obfuscate_word(unsigned char* buf, const struct word_data* wd){
	/*
	level 0: all asterisks
	level 1:
	5-		: *****		(0, 0)
	6		: a*****	(1, 0)
	7		: a******	(1, 0)
	8		: a******h	(1, 1)
	9		: a*******i	(1, 1)
	10		: ab*******j	(2, 1)
	11		: ab********k	(2, 1)
	12+		: ab********kl	(2, 2)
	level 2:
	4-		: ****		(0, 0)
	5		: a****		(1, 0)
	6		: a*****	(1, 0)
	7		: a*****g	(1, 1)
	8		: ab*****h	(2, 1)
	9		: ab******i	(2, 1)
	10		: ab******ij	(2, 2)
	11		: ab*******jk	(2, 2)
	12+		: ab********kl	(2, 2)
	13+		: abc********lm	(3, 2)
	level 3:
	3-		: ***		(0, 0)
	4		: a***		(1, 0)
	5		: a****		(1, 0)
	6		: ab****	(2, 0)
	7		: ab****g	(2, 1)
	8		: ab****gh	(2, 2)
	9		: ab*****hi	(2, 2)
	10		: ab*****hij	(2, 3)
	11		: ab******ijk	(2, 3)
	12+		: abc******jkl	(3, 3)
	level 4:
	2-		: **		(0, 0)
	3		: a**		(1, 0)
	4		: a***		(1, 0)
	5		: ab***		(2, 0)
	6		: ab***f	(2, 1)
	7		: ab***fg	(2, 2)
	8		: abc***gh	(3, 2)
	9		: abc****hi	(3, 2)
	10		: abc****hij	(3, 3)
	11		: abc*****ijk	(3, 3)
	12+		: abcd*****jkl	(4, 3)
	level 5:
	2-		: **		(0, 0)
	3		: a**		(1, 0)
	4		: ab**		(2, 0)
	5		: a***e		(1, 1)
	6		: ab***f	(2, 1)
	7		: abc***g	(3, 1)
	8		: abc***gh	(3, 2)
	9		: abc***ghi	(3, 3)
	10		: abc****hij	(3, 3)
	11		: abcd****ijk	(4, 3)
	12		: abcd*****jkl	(4, 3)
	13+		: abcd*****jklm	(4, 4)
	*/
	int len, before, after;
	len = strlen(word);
	min_update(len, 13);
	memcpy(buf, word, len + 1);
	if (wd->obfuscation_level != 0){
		before = OBFUSCATE_LENS[wd->obfuscation_level - 1][len] >> 4;
		after = OBFUSCATE_LENS[wd->obfuscation_level - 1][len] & 0xf;
		memset(buf + before, '*', len - before - after);
	}
}

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

#define word_list_has(wl, word) word_list_has_base(wl, word, false)

int send_word(const char* word, struct tile* board, int len){
	struct gwl_struct gwls;
	int ret;
	ret = word_list_has_base(global_wl, word, true);
	if (ret == WORD_FOUND){
		// Word is valid, decrement freqs
		gwls.board = board;
		gwls.len = len;
		pthread_mutex_init(&gwls.board_lock);
		gwls.inc = -1;
		gwl_mark_words(&gwls, buffer);
		pthread_mutex_destroy(&gwls.board_lock);
		return ret;
	}
	if (ret != WORD_NOT_FOUND){
		return ret;
	}
	return word_list_has_base(global_bonus, word, true) | WORD_BONUS;
}

