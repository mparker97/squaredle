#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include "file.h"

#define IDX_INIT_SZ 8
#define COMP_NBITS 5

/*
	A compressed word file contains the following:
	- A 4 byte unsigned int containing file position of the start of the index.
	- The words first sorted in alphabetical order then with their prefixes removed.
	- The index, which contains a list of entries of <string, uint> pairs of the
		indexed prefix and the file position where the words with that prefix start.
		These entries are sorted alphabetically by the prefix.
*/

int index_init(struct index* idx, int idx_key_len){
	idx->ls = malloc(sizeof(struct index_entry) * IDX_INIT_SZ);
	idx->sz = 0;
	idx->key_len = idx_key_len;
	idx->file_pos = 0;
}

void index_deinit(struct index* idx){
	int i;
	for (i = 0; i < idx->sz; i++){
		free(idx->ls[i].key);
	}
	free(idx->ls);
}

int index_add(struct index* idx, char* prefix, unsigned int pos){
	if (!(idx->sz & (idx->sz - 1)) && idx->sz >= IDX_INIT_SZ){
		idx->ls = realloc(sizeof(struct index_entry) * idx->sz * 2);
		if (idx->ls == NULL){
			return 1;
		}
	}
	idx->ls[idx->sz].key = strndup(prefix, idx->key_len);
	if (idx->ls[idx->sz].key == NULL){
		// TODO: strndup failed
		return 1;
	}
	idx->ls[idx->sz].value = pos;
	idx->sz++;
	return 0;
}

// Given details in 'cfg', compress an uncompressed word file
int compress_word_file(const struct compress_cfg* cfg){
	// TODO: what if cfg->min_word_len < cfg->idx_key_len?
	struct index idx;
	FILE* src;
	FILE* dst0;
	FILE* dst1;
	char* buf;
	char* prefix;
	unsigned int i, ret;
	char c;
	ret = 1;
	if (index_init(&idx, cfg->idx_key_len) != 0){
		// TODO
		goto fail0;
	}
	src = fopen(cfg->word_file, "r");
	if (src == NULL){
		// TODO
		goto fail1;
	}
	dst = fopen(cfg->compress_file, "x");
	if (dst == NULL){
		// TODO
		goto fail2;
	}
	buf = malloc(cfg->max_word_len);
	if (buf == NULL){
		// TODO:
		goto fail3;
	}
	prefix = malloc(idx.key_len + 1);
	if (prefix == NULL){
		// TODO:
		goto fail;
	}
	// Write space for the unsigned int giving the position of the index
	memset(prefix, 0, sizeof(prefix));
	fwrite(&i, sizeof(i), 1,  dst);
	// Go through words by char
	for (;;){
		for (i = 0; i < cfg->max_word_len; i++){
			buf[i] = fgetc(src);
			if (buf[i] == '\n'){
				// End of word; check if it is long enough
				if (i >= cfg->min_word_len){
					// Add prefix to index if it is different
					if (strncmp(buf, prefix, idx.key_len) != 0){
						memcpy(prefix, buf, idx.key_len);
						index_add(&idx, prefix, ftell(dst));
					}
					// Write word without prefix to compressed file
					fwrite(buf + idx.key_len, sizeof(char), i - idx.key_len, dst);
					fputc(dst, 0);
				}
				goto next_word;
			}
			else if (buf[i] == EOF){
				// Done reading words
				if (ferror(src) != 0){
					// TODO
					goto fail;
				}
				else if (i == 0){
					goto done;
				}
				else{
					// TODO
					goto fail;
				}
			}
			else{
				// Convert letter to uppercase
				c = make_uppercase(&buf[i]);
				if (c < 0){
					// TODO
					goto fail;
				}
				buf[i] = c;
			}
		}
		if (i == cfg->max_word_len){
			// TODO
			goto fail;
		}
next_word:
	}
done:
	// Mark position where index starts
	idx.file_pos = ftell(dst);
	// Write index
	fwrite(idx.ls, sizeof(struct index_entry), idx.sz, dst);
	if (ferror(dst)){
		// TODO
		goto fail;
	}
	// Write position where index starts to saved space at beginning of file
	fseek(dst, 0, SEEK_SET);
	fwrite(&idx.file_pos, sizeof(idx.file_pos), 1, dst);
	if (ferror(dst)){
		// TODO
		goto fail;
	}
	ret = 0;
fail:
	free(prefix);
fail3:
	free(buf);
fail2:
	fclose(dst);
fail1:
	fclose(src);
fail0:
	index_deinit(&idx);
}

// Binary search for the entry in the index 'idx' corresponding to 'word'
struct index_entry* index_bin_search(const struct index* idx, const char* word){
	unsigned int low, high, mid, cmp;
	low = 0;
	high = idx->sz;
	while (low < high){
		mid = (low + high) / 2;
		cmp = strncmp(word, idx->ls[mid].key, idx->key_len - 1);
		if (cmp < 0){
			high = mid;
		}
		else if (cmp > 0){
			low = mid + 1;
		}
		else{
			return &idx->ls[mid];
		}
	}
	// TODO: error; not found
	return NULL;
}

// Return in 'low_pos' and 'high_pos' the position bounds in the file where 'word' may be located
int word_file_idx_pos(const struct word_file* wf, const char* word, unsigned int* low_pos, unsigned int* high_pos){
	struct index_entry* x = index_bin_search(&wf->idx, word);
	if (x == NULL){
		// TODO: error
		return 1;
	}
	*low_pos = x[0].value;
	if (x == &wf->idx.ls[wf->idx.sz - 1]){
		// Last index entry, upper bound is index position
		*high_pos = wf->idx.file_pos;
	}
	else{
		*high_pos = x[1].value;
	}
	return 0;
}

// Return 0 if 'word' is a valid word, otherwise 1
int word_valid(const struct word_file* wf, const char* word){
	unsigned int low, high, mid;
	int i, j, k;
	char buf[wf->max_word_len + 2];
	if (word_file_idx_pos(wf, word, &low, &high) < 0){
		// TODO: fail
		return -1;
	}
	// Binary search within [low, high) file position range for word
	base = low;
	while (low < high){
		memset(buf, wf->max_word_len + 1, 0);
		mid = low + (high - low) / 2;
		fseek(wf->f, mid + 1, SEEK_SET);
		i = wf->max_word_len;
		// Search in reverse for end of previous word
		do{
			if (i < 0){
				// TODO: word in file too long
			}
			fseek(wf->f, -2, SEEK_CUR);
			buf[i] = fgetc(wf->f);
		}
		while (buf[i--] != 0);
		// Compare what we have so far; and skip if it doesn't match
		j = strncmp(&buf[i + 2], word + wf->idx.key_len, wf->max_word_len - (i + 2) + 1);
		if (j == 0){
			// They match so far; shift buffer contents to the beginning
			for (; i + 2 + j < wf->max_word_len + 1; j++){
				buf[j] = buf[i + 2 + j];
			}
			// Advance forward to the end of the word and compare
			fseek(wf->f, mid, SEEK_SET);
			fread(&buf[j], sizeof(char), wf->max_word_len - j, wf->f);
			k = strlen(&buf[j + 1]); // find end of string to skip over it if we recurse high
			j = strcmp(&buf[j + 1], &word[wf->idx.key_len + j + 1]);
			if (j == 0){
				return 1; // Matches; word found
			}
		}
		if (j < 0){
			low = mid + k;
		}
		else{
			high = mid - (wf->max_word_len - i + 2);
		}
	}
	return 0;
}

