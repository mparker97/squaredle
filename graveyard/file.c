#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include "file.h"

#define COMP_NBITS 5

struct line_stream{
	char* buf; // malloc'd
	char* curr; // ptr in buf
	int fd;
	int buf_sz;
};

struct index_entry{
	char key[4];
	unsigned int value;
}

struct word_file{
	struct index_entry* idx;
	unsigned int idx_len;
	unsigned int file_sz;
	int fd;
};

int line_stream_init(struct line_stream* ls, char* f, int buf_sz){
	ls->buf = malloc(buf_sz);
	if (ls->buf == NULL){
		return -1
	}
	ls->fd = open(f, O_RDONLY);
	if (ls->fd < 0){
		free(ls->buf);
		return -1;
	}
	ls->buf_sz = buf_sz;
	ls->curr = &ls->buf[buf_sz - 1];
	ls->curr[0] = 0;
	return 0;
}

void line_stream_deinit(struct line_stream* ls){
	free(ls->buf);
	if (ls->fd > 0){
		close(ls->fd);
	}
}

void line_stream_reset(struct line_stream* ls){
	seek(ls->fd, 0, SEEK_SET);
	ls->curr = &ls->buf[buf_sz - 1];
}

char* line_stream_next(struct line_stream* ls){
	char* s, *t;
	ssize_t nb;
	int i;
	for (;;){
		s = strchr(ls->curr, '\n');
		if (s){
			// newline found; tokenize
			*s = 0;
			t = ls->curr
			ls->curr = s + 1;
			return t;
		}
		if (ls->curr == ls->buf){
			// TODO: word is longer than buffer
			return NULL;
		}
		// (memcpy curr to buf)
		for (i = 0; ls->curr[i]; i++){
			ls->buf[i] = ls->curr[i];
		}
		nb = read(ls->fd, &ls->buf[i], ls->buf_sz - i - 1);
		if (nb < 0){
			// TODO: read failed
			return NULL;
		}
		if (nb == 0 && i == 0){
			// done
			return NULL;
		}
		if (nb < ls->buf_sz - i - 1){
			if (ls->buf[i + nb - 1] != '\n'){
				ls->buf[i + nb] = '\n';
				nb++;
			}
			ls->buf[i + nb] = 0;
		}
		ls->curr = ls->buf;
	}
}

int compress_word_file(struct compress_cfg* cfg){
	struct line_stream ls;
	FILE* cfile_idx;
	FILE* cfile_data;
	char sz_buf[4];
	char* s;
	char* idx;
	int i, prefix_streak;

	if (line_stream_init(&ls, cfg->word_file, cfg->max_word_len + 2) < 0){
		goto cleanup;
	}
	cfile_data = fopen(cfg->compress_file, 'x');
	if (cfile_data == NULL){
		// TODO
		return -1;
	}
	idx = malloc((compress_cfg->idx_len + 1) * sizeof(char));
	if (idx == NULL){
		// TODO
	}

	memset(idx, 0, (compress_cfg->idx_len + 1) * sizeof(char));
	memset(sz_buf, 0, sizeof(sz_buf));
	prefix_streak = 0;
	for (prefix_streak = 0; s = line_stream_next(&ls); prefix_streak++){
		for (i = 0; s[i]; i++){
			if (s[i] >= 'a' && s[i] <= 'z'){
				s[i] -= ('a' - 'A');
			}
			else if (s[i] < 'A' || s[i] > 'Z'){
				printf("Invalid character in source file: %c\n", s[i]);
				goto error;
			}
			ht_inc(&cfg->letter_ht, s[i] - 'A');
		}
		if (!strncmp(idx, s, compress_cfg->idx_len * sizeof(char))){
			// TODO: new index
			ht_dyn_inc(&cfg->len_ht, prefix_streak); // TODO: prefix streak is inaccurate here
			memcpy(idx, s, compress_cfg->idx_len * sizeof(char));
			// TODO: check fwrite rc
			fwrite(s, compress_cfg->idx_len + 1, sizeof(char), cfile_data);
			fwrite(sz_buf, 4, sizeof(char), cfile_data);
			prefix_streak = 0;
		}
	}

	cfile_idx = fopen(cfg->compress_file, 'x');
	if (cfile_idx == NULL){
		// TODO
		return -1;
	}
	line_stream_reset(&ls);
	memset(idx, 0, sizeof(idx));
	while (s = line_stream_next(&ls)){
		if (!strncmp(idx, s, compress_cfg->idx_len * sizeof(char))){
			i = ftell(cfile_data);
			fseek(cfile_idx, (compress_cfg->idx_len + 1) * sizeof(char), SEEK_CUR);
			fwrite(&i, 4, sizeof(char), cfile_idx); // TODO: error?
		}
		ht_write_string(&cfg->letter_ht, cfile_data, s); // TODO: error?
	}

	goto cleanup;
error:
cleanup:
	fclose(cfile_idx);
	fclose(cfile_data);
	line_stream_deinit(&ls);
}

struct index_entry* index_bin_search(struct word_file* wf, char* word){
	unsigned int low, high, mid, cmp;
	low = 0;
	high = wf->idx_len;
	while (low < high){
		mid = (low + high) / 2;
		cmp = strncmp(word, wf->idx[mid].key, 3);
		if (cmp < 0){
			high = mid;
		}
		else if (cmp > 0){
			low = mid + 1;
		}
		else{
			return &wf->idx[mid];
		}
	}
	// TODO: error; not found
	return NULL;
}

int word_file_idx_poses(struct word_file* wf, char* word, unsigned int* low_pos, unsigned int* high_pos){
	struct index_entry* idx = index_bin_search(wf, word);
	if (idx == NULL){
		// TODO: error
		return -1
	}
	*low_pos = idx[0].value;
	if (idx == &wf->idx[wf->idx_len]){
		*high_pos = wf->file_sz;
	}
	else{
		*high_pos = idx[1].value;
	}
	return 0
}

int word_valid(struct word_file* wf, char* word){
	unsigned int base, low, high, mid, i;
	int bit, old_bit;
	char buf[MAX_WORD_LEN + 1]; // TODO
	char b0, b1;
	if (word_file_idx_pos(wf, word, &low, &high) < 0){
		// TODO: fail
		return -1;
	}
	base = low;
	while (low < high){
		mid = low + (high - low) / 2;
		old_bit = bit = ((mid - base) * BPB) % COMP_NBITS;
		seek(wf->fd, mid, SEEK_SET);
		memset(buf, MAX_WORD_LEN, 0);
		b1 = getc(wf->fd);
		for (i = MAX_WORD_LEN - 1;; i--){
			if (i == 0){
				// TODO: word too long
				return -1;
			}
			if (bit < COMP_NBITS){
				seek(wf->fd, -2, SEEK_CUR);
				b0 = getc(wf->fd);
				buf[i] = 'A' + (b0 >> (BPB - (COMP_NBITS - bit))) | (b1 & ((1 << bit) - 1)) << (COMP_NBITS - bit);
				b1 = b0;
			}
			else{
				buf[i] = 'A' + ((b1 >> (bit - COMP_NBITS)) & ((1 << COMP_NBITS) - 1));
			}	
			if (buf[i] == 'Z' + 1){
				for (j = 0, i++; buf[i + j]; j++){
					if (word[j] < buf[i + j]){ // TODO: remove prefix?
						// if length of word is too short, word[j] = 0 should've failed this check
						high = mid - ((MAX_WORD_LEN - 1 - (i - 2)) * COMP_NBITS + (BPB - 1)) / BPB;
						goto next_while;
					}
					else if (word[j] > buf[i + j]){
						low = mid + 1;
						goto next_while;
					}
					buf[j] = buf[i + j];
				}
				break;
			}
			bit = (bit + BPB - COMP_NBITS) % BPB;
		}
		bit = old_bit;
		seek(wf->fd, mid, SEEK_SET);
		b0 = getc(wf->fd);
		for (; j < MAX_WORD_LEN; j++){
			if (bit > BPB - COMP_NBITS){
				b1 = getc(wf->fd);
				buf[j] = 'A' + (b0 >> bit) | (b1 << (BPB - bit));
				b0 = b1;
			}
			else{
				buf[j] = 'A' + ((b0 >> bit) & ((1 << COMP_NBITS) - 1));
			}
			if (word[j] < buf[j]){
				// if length of word is too short, word[j] = 0 should've failed this check
				high = mid - ((MAX_WORD_LEN - 1 - (i - 2)) * COMP_NBITS + (BPB - 1)) / BPB;
				goto next_while;
			}
			else if (word[j] > buf[j]){
				low = mid + ((MAX_WORD_LEN - 1 - (i - 1) + (j + 1)) * COMP_NBITS + (BPB - 1)) / BPB;
				goto next_while;
			}
			else if (word[j] == 0){
				// TODO: found it
				return 1;
			}
			bit = (bit + BPB - COMP_NBITS) % BPB;
		}
		// TODO: word too long
		return -1;
next_while:
	}
	// didn't find it
	return 0;
}

