#include "huffman_tree.h"

#define BPB 8 // bits per byte

struct huffman_tree_element{
	struct huffman_tree_node* next;
	unsigned int freq;
	char c;
};

struct huffman_tree{
	// TODO: chars with codes
	struct huffman_tree_node* tree;
	int num_chars;
};

void ht_init(struct huffman_tree* ht, char num_chars){
	// num_chars must be < 86
	ht->tree = calloc(num_chars, sizeof(struct huffman_tree_node));
	if (ht->tree == NULL){
		// TODO: calloc failed
	}
	ht->num_chars = num_chars;
}

int ht_inc(struct huffman_tree* ht, char c){
	
}

int ht_get_char(struct huffman_tree* ht, char* buf, int bit, char c){
	// TODO: return bit + BPB - 1 when c == 0
}

int ht_write_string(struct huffman_tree* ht, FILE* f, char* s){
	char buf[8];
	int nbits;
	int i, j, bit;
	i = 0;
	bit = 0;
	do {
		nbits = ht_get_char(ht, buf, bit, s[i]);
		if (nbits == bit){
			// TODO: didn't find char
			return -1;
		}
		for (j = 0; j < nbits / BPB; j++){
			fputc(f, buf[j]);
		}
		buf[0] = buf[nbits / BPB];
		bit = nbits % BPB;
	} while (s[i++]);
	return 0;
}


