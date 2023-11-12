/*
Options:
	sanitize FILE
	cull FILE SUBFILE
	alpha FILE out
	stats FILE
	substring FILE SUBSTRING
	index FILE INDEX_LEN
	length FILE MIN_LEN [MAX_LEN]
	isogram FILE
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//#define DEBUG

#define MAX_WORD_LEN 256
#define WORD_BUFFER(v) char v[MAX_WORD_LEN + 1]

void capitalize(char* c){
	if (*c >= 'a' && *c <= 'z'){
		*c -= 'a' - 'A';
	}
}

int next_word_base(FILE* f, char* buf, int make_caps){
	int i;
	char c;
	if (feof(f)){
		buf[0] = '\xff';
		buf[1] = 0;
		return 0;
	}
loop:
	for (i = 0;; i++){
		switch(buf[i] = fgetc(f)){
			case EOF:
				buf[i] = 0;
				return 0;
			case '\n':
				buf[i] = 0;
				while ((c = fgetc(f)) == '\n');
				if (c == EOF){
					return 0;
				}				
				ungetc(c, f);
				return 1;
		}
		if (make_caps){
			// Make caps
			capitalize(&buf[i]);
		}
		if (i == MAX_WORD_LEN){
			buf[i] = 0;
			fprintf(stderr, "Word too long: %s...\n", buf);
			// exhaust remainder of word
			switch (fgetc(f)){
				case '\n':
					goto loop; // start over
				case EOF:
					buf[0] = 0;
					return 0;
			}
		}
	}
}

#define next_word(x, y) next_word_base(x, y, 1)

int sanitize_func(int argc, char* argv[], FILE* wl){
	// Remove words starting with a capital letter
	// Remove single-letter words except 'a'
	// Remove words ending in "'s"
	// Capitalize all letters
	WORD_BUFFER(buf);
	size_t l;
	int i, cont;
	unsigned int total, count;
	if (argc != 3){
		fprintf(stderr, "Usage: %s %s FILE\n", argv[0], argv[1]);
		return 1;
	}
	total = count = 0;
	for (cont = 1; cont; total++){
		cont = next_word_base(wl, buf, 0);
		if (buf[0] >= 'A' && buf[0] <= 'Z'){
			continue;
		}
		l = strlen(buf);
		if (l == 1){
			if (buf[0] != 'a'){
				continue;
			}
		}
		else if (buf[l - 2] == '\'' && buf[l - 1] == 's'){
			continue;
		}
		for (i = 0; buf[i]; i++){
			capitalize(&buf[i]);
		}
		printf("%s\n", buf);
		count++;
	}
	fprintf(stderr, "%u words removed, %u words remain\n", total - count, count);
	return 0;
}

int cull_func(int argc, char* argv[], FILE* file){
	WORD_BUFFER(buf_file);
	WORD_BUFFER(buf_subfile);
	FILE* subfile;
	int i, cont, c;
	int skip;
	unsigned int count;
	if (argc != 4){
		fprintf(stderr, "Usage: %s %s FILE SUBFILE\n", argv[0], argv[1]);
		return 1;
	}
	subfile = fopen(argv[3], "r");
	if (subfile == NULL){
		fprintf(stderr, "Failed to open file %s\n", argv[3]);
		return 1;
	}
	// Remove words in file that are in subfile
	count = 0;
	c = 0;
	skip = 0;
	for (cont = 1; cont;){
		cont = 0;
		if (c <= 0){
			cont |= next_word(subfile, buf_subfile);
		}
		if (c >= 0){
			cont |= next_word(file, buf_file);
		}
		c = strcmp(buf_subfile, buf_file);
		if (c > 0){
			printf("%s\n", buf_file);
		}
		else if (c < 0){
			fprintf(stderr, "%s is in %s but not in %s\n", buf_subfile, argv[3], argv[2]);
		}
		else{
			count++;
		}
	}
	fclose(subfile);
	fprintf(stderr, "Culled %u words from %s\n", count, argv[2]);
	return 0;
}

static inline int letter_is_alpha(char l){
	if (l < 'A' || (l > 'Z' && l < 'a') || l > 'z'){
		return 0;
	}
	return 1;
}

int word_is_alpha(const char* s){
	int i;
	for (i = 0; s[i]; i++){
		if (!letter_is_alpha(s[i])){
			return 0;
		}
	}
	return 1;
}

int alpha_func(int argc, char* argv[], FILE* wl){
	WORD_BUFFER(buf);
	WORD_BUFFER(input);
	FILE* out;
	char* str;
	int i, cont, skip;
	if (argc != 4){
		fprintf(stderr, "Usage: %s %s FILE OUT\n", argv[0], argv[1]);
		return 1;
	}
	out = fopen(argv[3], "w");
	if (out == NULL){
		fprintf(stderr, "Failed to open file %s\n", argv[3]);
		return 1;
	}

	// Interactive loop that reports each problematic word
	printf("Type each word's replacement (blank = delete word; single space = quit):\n");
	skip = 0;
	for (cont = 1; cont;){
		cont = next_word(wl, buf);
		if (!skip && !word_is_alpha(buf)){
			do {
				printf("%s: ", buf);
				fflush(stdin);
				if (fgets(input, MAX_WORD_LEN, stdin) == NULL){
					fprintf(stderr, "fgets failed\n");
					fclose(out);
					return 1;
				}
				if (input[0] == ' ' && input[1] == '\n'){ // quit program
					skip = 1;
					goto next;
				}
				else if (input[0] == '\n'){ // delete word
					goto next;
				}
				str = NULL;
				for (i = 0; i < MAX_WORD_LEN; i++){
					if (input[i] == '\n'){
						str = &input[i];
						*str = 0;
						break;
					}
					if (!letter_is_alpha(input[i])){
						break;
					}
					capitalize(&input[i]);
				}
			} while (str == NULL);
			fprintf(out, "%s\n", input);
		}
		else {
			fprintf(out, "%s\n", buf);
		}
next:;
	}
	fclose(out);
	return 0;
}

struct stats{
	unsigned int n_words;
	unsigned int n_letters;
	unsigned int len_freqs[MAX_WORD_LEN];
	unsigned int start_freqs[26];
	unsigned int letter_freqs[26];
	unsigned int n_problematic_words;
};

int stats_func(int argc, char* argv[], FILE* wl){
	WORD_BUFFER(buf);
	struct stats sts;
	int i, cont = 1;
	int prob;
	if (argc != 3){
		fprintf(stderr, "Usage: %s %s FILE\n", argv[0], argv[1]);
		return 1;
	}
	memset(&sts, 0, sizeof(sts));
	for (; cont;){
		cont = next_word(wl, buf);
		prob = 0;
		sts.n_words++;
		for (i = 0; buf[i]; i++){
			if (buf[i] < 'A' || buf[i] > 'Z'){
				prob = 1;
				continue;
			}
			sts.n_letters++;
			sts.letter_freqs[buf[i] - 'A']++;
			if (i == 0){
				sts.start_freqs[buf[i] - 'A']++;
			}
		}
		sts.len_freqs[i]++;
		sts.n_problematic_words += prob;
	}
	printf("Number of words: %u\n", sts.n_words);
	printf("Number of problematic words: %u\n", sts.n_problematic_words);
	printf("Number of letters: %u\n", sts.n_letters);
	printf("Length frequencies:\n");
	for (i = 0; i < MAX_WORD_LEN; i++){
		if (sts.len_freqs[i] > 0){
			printf("\t%d: %u (%.2f%%)\n", i, sts.len_freqs[i], (double)(sts.len_freqs[i]) * 100 / sts.n_words);
		}
	}
	printf("Letter frequencies, letter start frequencies:\n");
	for (i = 0; i < 26; i++){
		printf("\t%c: %u (%.2f%%), %u (%.2f%%)\n", i + 'A', sts.letter_freqs[i],
					 (double)(sts.letter_freqs[i]) * 100 / sts.n_letters, sts.start_freqs[i],
					 (double)(sts.start_freqs[i]) * 100 / sts.n_words);
	}
	return 0;
}

int substring_func(int argc, char* argv[], FILE* wl){
	WORD_BUFFER(buf);
	char* str;
	size_t l;
	int i, cont;
	unsigned int count;
	if (argc != 4){
		fprintf(stderr, "Usage: %s %s FILE SUBSTRING\n", argv[0], argv[1]);
		return 1;
	}
	str = argv[3];
	l = strlen(str);
	if (l > MAX_WORD_LEN){
		fprintf(stderr, "Substring \"%s\" is too long\n", str);
		return 1;
	}
	for (i = 0; str[i]; i++){
		capitalize(&str[i]);
	}
	count = 0;
	for (cont = 1; cont;){
		cont = next_word(wl, buf);
		if (strstr(buf, str)){
			printf("%s\n", buf);
			count++;
		}
	}
	fprintf(stderr, "Number of words with substring \"%s\": %u\n", str, count);
	return 0;
}

// Return 1 if 'b' exists in 'a' with at most 'n_err' errors
// Assume n_err < strlen(b) < MAX_WORD_LEN
int is_substring_with_errors(char* a, char* b, int n_err){
	int mat_[MAX_WORD_LEN + 1];
	int* mat;
	char str[MAX_WORD_LEN * 2];
	ssize_t lena, lenb;
	int ia, ib;
	int tmp, new, gmin;
	int flag = 0;
	mat = &mat_[1];
	mat_[0] = MAX_WORD_LEN;
	lena = strlen(a);
	lenb = strlen(b);
	memset(str, 0, MAX_WORD_LEN * 2);
	strcpy(str + n_err, a);
	gmin = MAX_WORD_LEN;
	// Clear matrix (to infinitys)
	for (ib = 0; ib < lenb; ib++){
		mat[ib] = MAX_WORD_LEN;
	}
	// Loop through columns (chars of a)
	for (ia = 0; ia < lena + n_err * 2; ia++){ 
		new = 0;
		// Loop through rows (chars of b)
		for (ib = 0; ib < lenb; ib++){
			// Calculate min of the following three:
			if (str[ia] != b[ib]){ // C[i-1, j-1] + d(i, j)
				new++;
			}
			if (mat[ib - 1] + 1 < new){ // C[i, j-1] + 1
				new = mat[ib - 1] + 1;
			}
			if (mat[ib] + 1 < new){ // C[i, j-1] + 1
				new = mat[ib] + 1;
			}
			tmp = mat[ib];
			mat[ib] = new;
			new = tmp;
		}
		if (mat[lenb - 1] <= n_err){
			// Found suitable substring
			if (mat[lenb - 1] < gmin){
				gmin = mat[lenb - 1];
				if (gmin == 0){
					return gmin;
				}
			}
		}
	}
	if (gmin < MAX_WORD_LEN){
		return gmin;
	}
	return -1;
}

int q_substring_func(int argc, char* argv[], FILE* wl){
	WORD_BUFFER(buf);
	char* str;
	char* endptr;
	size_t l;
	int i, cont;
	int n_err;
	unsigned int count;
	if (argc != 5){
		fprintf(stderr, "Usage: %s %s FILE SUBSTRING N_ERR\n", argv[0], argv[1]);
		return 1;
	}
	str = argv[3];
	l = strlen(str);
	if (l > MAX_WORD_LEN){
		fprintf(stderr, "Substring \"%s\" is too long\n", str);
		return 1;
	}
	n_err = strtol(argv[4], &endptr, 10);
	if (*endptr != '\0' || n_err < 0){
		fprintf(stderr, "Invalid value for N_ERR: %s\n", argv[4]);
		return 1;
	}
	if (n_err == 0){
		return substring_func(argc - 1, argv, wl);
	}
	if (n_err >= l){
		fprintf(stderr, "N_ERR of %d yields all words\n", n_err);
		return 1;
	}
	for (i = 0; str[i]; i++){
		capitalize(&str[i]);
	}
	count = 0;
	for (cont = 1; cont;){
		cont = next_word(wl, buf);
		i = is_substring_with_errors(buf, str, n_err);
		if (i >= 0){
			printf("%s (%d)\n", buf, i);
			count++;
		}
	}
	fprintf(stderr, "Number of words with substring \"%s\" up to %d errors: %u\n", str, n_err, count);
	return 0;
}

int index_func(int argc, char* argv[], FILE* wl){
	WORD_BUFFER(buf);
	WORD_BUFFER(prefix);
	int i, cont;
	int prefix_len;
	unsigned int count;
	if (argc != 3){
		fprintf(stderr, "Usage: %s %s PREFIX_LEN\n", argv[0], argv[1]);
		return 1;
	}
	prefix_len = atoi(argv[2]);
	if (prefix_len < 1 || prefix_len >= MAX_WORD_LEN - 1){
		fprintf(stderr, "Invalid prefix length %d\n", prefix_len);
		return 1;
	}
	memset(prefix, 0, prefix_len + 1);
	count = 0;
	for (cont = 1; cont;){
		cont = next_word(wl, buf);
		if (strlen(buf) < prefix_len){
			continue;
		}
		if (strncmp(prefix, buf, prefix_len) < 0){
			if (count > 0){ // just to skip first
				printf("%s: %u\n", prefix, count);
			}
			memcpy(prefix, buf, prefix_len);
			count = 0;
		}
		count++;
	}
	printf("%s: %u\n", prefix, count);
	return 0;
}

int length_func(int argc, char* argv[], FILE* wl){
	WORD_BUFFER(buf);
	size_t l;
	int cont;
	int min, max;
	unsigned int total, count;
	max = MAX_WORD_LEN;
	if (argc == 5){
		max = atoi(argv[4]);
		if (max < 1){
			fprintf(stderr, "Invalid maximum length %d\n", max);
			return 1;
		}
	}
	else if (argc != 4){
		fprintf(stderr, "Usage: %s %s FILE MIN_LEN [MAX_LEN]\n", argv[0], argv[1]);
    return 1;
	}
	min = atoi(argv[3]);
	if (min > MAX_WORD_LEN - 1){
		fprintf(stderr, "Invalid minimum length %d\n", min);
    return 1;
	}
	if (min > max){
		fprintf(stderr, "Minimum length %d is greater than maximum length %d\n", min, max);
		return 1;
	}
	total = count = 0;
	for (cont = 1; cont; total++){
		cont = next_word(wl, buf);
		l = strlen(buf);
		if (l >= min && l <= max){
			printf("%s\n", buf);
			count++;
		}
	}
	fprintf(stderr, "%u out of %u words (%.2f%%) that fit the length constraint\n",
				  count, total, (double)count * 100 / total);
	return 0;
}

int isogram_func(int argc, char* argv[], FILE* wl){
	WORD_BUFFER(buf);
	int i, cont;
	unsigned int bm;
	unsigned int count;
	count = 0;
	for (cont = 1; cont;){
		cont = next_word(wl, buf);
		bm = 0;
		for (i = 0; buf[i]; i++){
			if (buf[i] < 'A' || buf[i] > 'Z'){
				goto next;
			}
			if (bm & (1 << (buf[i] - 'A'))){
				goto next;
			}
			bm |= (1 << (buf[i] - 'A'));
		}
		printf("%s\n", buf);
		count++;
next:
		bm = 0; // no-op
	}
	fprintf(stderr, "%u isograms\n", count);
	return 0;
}

/*
	char r;
	while ((r = getopt(argc, argv, "TODO")) != -1){
	}
*/

char* SUBCOMMAND_NAMES[] = {
	"sanitize",
	"cull",
	"alpha",
	"stats",
	"substring",
	"qsubstring",
	"index",
	"length",
	"isogram"
};

int (*SUBCOMMAND_FUNCS[])(int argc, char* argv[], FILE* wl) = {
	sanitize_func,
	cull_func,
	alpha_func,
	stats_func,
	substring_func,
	q_substring_func,
	index_func,
	length_func,
	isogram_func
};

int main(int argc, char* argv[]){
	FILE* wl;
	int i, ret;
	if (argc < 3){
		fprintf(stderr, "%s: Too few arguments\n", argv[0]);
		return 1;
	}
	wl = fopen(argv[2], "r");
	if (wl == NULL){
		fprintf(stderr, "Failed to open file %s\n", argv[2]);
		return 1;
	}
	for (i = 0; i < sizeof(SUBCOMMAND_NAMES) / sizeof(char*); i++){
		if (!strcmp(SUBCOMMAND_NAMES[i], argv[1])){
			ret = SUBCOMMAND_FUNCS[i](argc, argv, wl);
			goto end;
		}
	}
	fprintf(stderr, "Unknown operation \"%s\"\n", argv[1]);
	ret = 1;
end:
	fclose(wl);
	return ret;
}
