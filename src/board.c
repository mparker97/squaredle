#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "board.h"
#include "common.h"
#include "gwl.h"
#include "word_list.h"

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

struct tile_list{ // linked list
	struct tile* tile;
	struct tile_list* next;
};

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

