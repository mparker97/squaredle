#define ROUNDUP(x, b) ((((x) + (b) - 1) / (b)) * (b))

struct compress_cfg{
	char* word_file_path;
	char* compress_file_path;
	int min_word_len;
	int max_word_len;
	int idx_key_len;
};

// Entry of the word file index. 'key' is the prefix and 'value' is the position
// in the file where the words beginning with the prefix start.
struct index_entry{
	char* key; // heap
	unsigned int value;
}

// Word file index. Each entry in 'ls' has a key length of 'key_len'.
// The index is after the data in the word file, at position 'file_pos'.
struct index{
	struct index_entry* ls; // heap alist
	int sz;
	int key_len;
	unsigned int file_pos;
};

struct word_file{
	char* name;
	FILE* f;
	unsigned int file_sz;
	int max_word_len;
	struct index idx;
};

char make_uppercase(char c){
	if (c > 'Z'){
		c -= 'a' - 'A';
	}
	if (*c < 'A' || *c > 'Z'){
		// TODO
		return -1;
	}
	return c;
}
