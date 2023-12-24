/*
Options:
	sanitize FILE
	cull FILE SUBFILE
	alpha FILE out
	problematic FILE
	stats FILE
	substring FILE SUBSTRING
	qsubstring FILE SUBSTRING N_ERR
	index FILE INDEX_LEN
	length FILE MIN_LEN [MAX_LEN]
	isogram FILE
	compress FILE OUT INDEX_LEN
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

//#define DEBUG

#define MAX_WORD_LEN 256
#define WORD_BUFFER(v) char v[MAX_WORD_LEN + 1]

const char* ALPHA_STR = "AAAAAAACEEEEIIIIDNOOOOO OUUUUY  ";

int strdiff(const char* s0, const char* s1){
	int i;
	for (i = 0; s0[i] || s1[i]; i++){
		if (s0[i] != s1[i]){
			break;
		}
	}
	return i;
}

struct char_stream{
	char* str;
	size_t sz;
};

#define CHAR_STREAM_INIT_SZ (1 << 8)

int char_stream_init(struct char_stream* s){
	s->str = malloc(CHAR_STREAM_INIT_SZ * sizeof(char));
	if (s->str == NULL){
		return 0;
	}
	s->sz = 0;
	return 1;
}

void char_stream_deinit(struct char_stream* s){
	free(s->str);
	s->str = NULL;
}

int char_stream_write(struct char_stream* s, const char* n){
	unsigned int i;
	for (i = 0; n[i]; i++) {
		if (!(s->sz & (s->sz - 1)) && s->sz >= CHAR_STREAM_INIT_SZ){
			s->str = realloc(s->str, s->sz * 2);
			if (s->str == NULL){
				return 0;
			}
		}
		s->str[s->sz++] = n[i];
	}
	return 1;
}

int check_int_arg(char* arg, char* name, int min, int max, int* ret){
	char* endptr;
	*ret = strtol(arg, &endptr, 10);
	if (*endptr != '\0'){
		fprintf(stderr, "Invalid value for %s: %s\n", name, arg);
		return 0;
	}
	if (*ret < min){
		fprintf(stderr, "Invalid value for %s: %d (minimum is %d)\n", name, *ret, min);
		return 0;
	}
	if (*ret > max){
		fprintf(stderr, "Invalid value for %s: %d (maximum is %d)\n", name, *ret, max);
		return 0;
	}
	return 1;
}

void capitalize(char* c){
	if (c[0] >= 'a' && c[0] <= 'z'){
		c[0] -= 'a' - 'A';
	}
	else if (c[0] == '\xc3' && c[1] >= '\xa0' && c[1] <= '\xbd' && c[1] != '\xb7'){
		c[1] -= 0x20;
	}
}

void alphatize(char* c){
	int i, j;
	char d, m;
	for (i = 0; c[i]; i++){
		if (c[i] == '\xc3'){
			if (c[i + 1] == 0){
				continue;
			}
			d = (c[i + 1] - '\x80') / 32;
			m = (c[i + 1] - '\x80') % 32;
			if (d > 1){ // beyond category
				continue;
			}
			if (ALPHA_STR[m] == ' '){
				continue;
			}
			c[i] = ALPHA_STR[m] + d * 0x20;
			if (m == 0x06){ // ae
				c[i + 1] = 'E' + d * 0x20;
			}
			else {
				for (j = 1; c[i + j]; j++){
					c[i + j] = c[i + j + 1];
				}
			}
		}
	}
}

int next_word_base(FILE* f, char* buf, int make_caps){
	// TODO: return length of new word somehow?
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
	// Alphatize all letters
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
		//if (buf[0] >= 'A' && buf[0] <= 'Z'){
		//	continue;
		//}
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
		alphatize(buf);
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
		else {
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

int problematic_func(int argc, char* argv[], FILE* wl){
	WORD_BUFFER(buf);
	int cont;
	int count;
	if (argc != 3){
		fprintf(stderr, "Usage: %s %s FILE\n", argv[0], argv[1]);
		return 1;
	}
	count = 0;
	for (cont = 1; cont;){
		cont = next_word(wl, buf);
		if (!word_is_alpha(buf)){
			printf("%s\n", buf);
			count++;
		}
	}
	fprintf(stderr, "%d problematic words\n", count);
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

#define F_SUBSTRING_BA 1
#define F_SUBSTRING_EA 2

char* substring_flags(char* str, int* flags){
	size_t l;
	l = strlen(str);
	*flags = 0;
	if (str[l - 1] == '$'){
		str[l - 1] = 0;
		*flags |= F_SUBSTRING_EA;
	}
	if (str[0] == '^'){
		str++;
		*flags |= F_SUBSTRING_BA;
	}
	return str;
}

int substring_func(int argc, char* argv[], FILE* wl){
	WORD_BUFFER(buf);
	char* str, *p;
	size_t l_buf, l_str;
	int i, cont, flags;
	unsigned int count;
	if (argc != 4){
		fprintf(stderr, "Usage: %s %s FILE SUBSTRING\n", argv[0], argv[1]);
		return 1;
	}
	str = substring_flags(argv[3], &flags);
	l_str = strlen(str);
	if (l_str > MAX_WORD_LEN){
		fprintf(stderr, "Substring \"%s\" is too long\n", str);
		return 1;
	}
	for (i = 0; str[i]; i++){
		capitalize(&str[i]);
	}
	count = 0;
	for (cont = 1; cont;){
		cont = next_word(wl, buf);
		l_buf = strlen(buf);
		p = strstr(buf, str);
		if (p){
			if (flags & F_SUBSTRING_BA && p != buf){
				continue;
			}
			if (flags & F_SUBSTRING_EA && p != buf + l_buf - l_str){
				continue;
			}
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
	if (!check_int_arg(argv[4], "N_ERR", 0, l - 1, &n_err)){
		return 1;
	}
	if (n_err == 0){
		return substring_func(argc - 1, argv, wl);
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

#define MAX_PREFIX_LEN 8

int index_func(int argc, char* argv[], FILE* wl){
	WORD_BUFFER(buf);
	char prefix[MAX_PREFIX_LEN + 1];
	int i, cont;
	int prefix_len;
	unsigned int n_prefixes;
	unsigned int count;
	if (argc != 4){
		fprintf(stderr, "Usage: %s %s PREFIX_LEN\n", argv[0], argv[1]);
		return 1;
	}
	if (!check_int_arg(argv[3], "PREFIX_LEN", 1, MAX_PREFIX_LEN, &prefix_len)){
		return 1;
	}
	memset(prefix, 0, prefix_len + 1);
	count = n_prefixes = 0;
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
			n_prefixes++;
			count = 0;
		}
		count++;
	}
	printf("%s: %u\n", prefix, count);
	printf("%u prefixes\n", n_prefixes);
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
		if (!check_int_arg(argv[4], "MAX_LEN", 1, max, &max)){
			return 1;
		}
	}
	else if (argc != 4){
		fprintf(stderr, "Usage: %s %s FILE MIN_LEN [MAX_LEN]\n", argv[0], argv[1]);
    return 1;
	}
	if (!check_int_arg(argv[3], "MIN_LEN", 1, max, &min)){
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
	if (argc != 3){
		fprintf(stderr, "Usage: %s %s\n", argv[0], argv[1]);
		return 1;
	}
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

int compress_func(int argc, char* argv[], FILE* wl){
	WORD_BUFFER(buf);
	WORD_BUFFER(buf2);
	char prefix[MAX_PREFIX_LEN + sizeof(unsigned int) + 1];
	FILE* out;
	struct stat st;
	struct char_stream s;
	int prefix_len;
	int cflag, oflag;
	int cont, ret;
	int diff;
	off_t sz;
	unsigned int* pos;
	unsigned int uint;
	char c;
	cflag = oflag = 0;
	if (argc < 5){
		fprintf(stderr, "Usage: %s %s FILE OUT PREFIX_LEN [-co]\n", argv[0], argv[1]);
		return 1;
	}
	while ((c = getopt(argc - 4, &argv[4], "co")) != -1){
		switch (c){
			case 'c':
				cflag = 1;
				break;
			case 'o':
				oflag = 1;
				break;
			case '?':
				fprintf(stderr, "Unknown option '%c'\n", c);
				return 1;
		}
	}
	out = fopen(argv[3], "w+");
	if (out == NULL){
		fprintf(stderr, "Failed to open file %s\n", argv[3]);
		return 1;
	}
	s.str = NULL;
	ret = 1;
	if (!check_int_arg(argv[4], "PREFIX_LEN", 1, MAX_PREFIX_LEN, &prefix_len)){
		goto fail;
	}
	if (!char_stream_init(&s)){
		goto fail;
	}
	buf2[0] = 0;
	memset(prefix, 0, sizeof(prefix));
	pos = (unsigned int*)&prefix[prefix_len];
	diff = 0;
	fseek(out, sizeof(unsigned int), SEEK_SET);
	//fwrite(uint, sizeof(unsigned int), 1, out);
	for (cont = 1; cont;){
		cont = next_word(wl, buf);
		if (strlen(buf) < prefix_len){ // TODO
			continue;
		}
		if (strncmp(prefix, buf, prefix_len) < 0){
		//if (strcmp(prefix, buf) < 0){
			memcpy(prefix, buf, prefix_len);
			*pos = (unsigned int)ftell(out);
			if (!char_stream_write(&s, prefix)){
				fprintf(stderr, "str_stream_write failed\n");
				goto fail;
			}
			prefix[prefix_len] = 0;
		}
		if (oflag){
			diff = strdiff(buf, buf2);
			strcpy(buf2 + diff, buf + diff);
			if (diff < prefix_len){ // new prefix
				diff = 0;
			}
			else {
				fputc(diff, out);
				diff -= prefix_len;
			}
		}
		fprintf(out, "%s\n", buf + prefix_len + diff);
	}
	uint = ftell(out);
	fwrite(s.str, sizeof(char), s.sz, out);
	sz = ftell(out);
	fseek(out, 0, SEEK_SET);
	fwrite(&uint, sizeof(uint), 1, out);
	fstat(fileno(wl), &st);
	printf("Original size: %lld, compressed size: %lld (%.2f%%)\n", st.st_size, sz, ((double)sz / st.st_size - 1) * 100);
	ret = 0;
fail:
	fclose(out);
	char_stream_deinit(&s);
	return ret;
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
	"problematic",
	"stats",
	"substring",
	"qsubstring",
	"index",
	"length",
	"isogram",
	"compress"
};

int (*SUBCOMMAND_FUNCS[])(int argc, char* argv[], FILE* wl) = {
	sanitize_func,
	cull_func,
	alpha_func,
	problematic_func,
	stats_func,
	substring_func,
	q_substring_func,
	index_func,
	length_func,
	isogram_func,
	compress_func
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
