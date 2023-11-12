struct tile{
  struct tile** neighbors;
  int start_freq;
  int freq;
  char letter;
  char can_repeat;
  char neighors_len;
	char visited; // ref count
};

struct tile_list{
	struct tile* tile;
	struct tile_list* next;
};

struct word_list{
	char** list; // heap pointer to heap pointers
	int sz;
};

struct word_data{
	char* word;
	char uses_repeat;
	char uses_wildcard;
};

struct word_data_list{
	struct word_data* list; // heap
	int sz;
};

struct wlf_idx{
	int idx; // idx of the beginning of the NEXT length of words
					// This way, since MIN_WORD_LEN > 0, idx = 0 implies no words of this length
	int count;
};

struct word_list_final{
	struct word_list wl;
	struct word_list bonus;
	struct wlf_idx* idx;
	int idx_sz;
};

#define bitmap_set(bm, bit) ((bm)[(bit) / 8] |= (1 << ((bit) % 8)))
#define bitmap_get(bm, bit) ((bm)[(bit) / 8] & (1 << ((bit) % 8)))

#define ADJACENCY_BM_SZ 26
typedef unsigned int[ADJACENCY_BM_SZ] adjacency_bm_t;

int adjacency_bitmap_set(adjacency_bm_t bm, char x, char y){
	char c;
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
	else{
		bm[x - 'A'] |= 1 << (y - 'A');
	}
	return 0;
}

inline int adjacency_bitmap_get(const adjacency_bm_t bm, char x, char y){
	return bm[x - 'A'] & (1 << (y - 'A'));
}

// 'buf' must be a string of length >= 1
int adjacency_bitmap_all(const adjacency_bm_t bm, const char* buf){
	while (buf[1]){
		if (!adjacency_bitmap_get(bm, buf[0], buf[1])){
			return 0;
		}
		buf++;
	}
	return 1;
}

void obfuscate_word(char* buf, const char* word);
