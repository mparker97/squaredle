// Allow for usage of the program solely in a terminal when the board consists
// of only square tiles all of the same size.

/*

┏━━━┳━━━┓
┃ A ┃ B ┃
┃9 +┃1 2┃
┣━━━╋━━━┫
┃ C ┃   ┃
┃4 5┃   ┃
┗━━━┻━━━┛

*/

#include <ioctl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <wchar.h>
#include "board.h"
#include "common.h"

#define MAX_BOARD_SZ_C 50
#define MAX_BOARD_SZ_R 40

#define TB_TILE_SZ_C 5
#define TB_TILE_SZ_R 4

#define MODE_ENTRY 1
#define MODE_WL 2

static struct termios termios;
static struct termios old_termios;
static bool terminal_set = false;
static bool loop = true;

extern struct word_list global_wl;
extern struct word_list global_bonus;

struct terminal_data{
	struct tile* board;
	struct coord board_sz; // In tiles
	struct coord screen_sz;
	struct coord wl_sz;
	struct coord wl_pos;
	int wl_cursor_pos;
	word_buffer_t textbox;
	int textbox_pos;
	int textbox_cursor_pos;
	int mode;
};

void change_mode(struct terminal_data* td, int mode){
	if (td->mode == mode){
		return;
	}
	
}

int get_wl_starting_idx(struct terminal_data* td, int* idx, struct word_list** _wl){
	// if:
	// 	off = get_wl_starting_idx(td, &idx, &wl);
	// then the starting position is word 'off' in index 'idx' of word list 'wl':
	//	wl->ls[wl->idx[idx - 1] + off]
	struct word_list* wl;
	int i, count;
	register int words_per_line, words_of_len, tmp;
	for (wl = &global_wl;; wl = &global_bonus){
		for (i = MIN_WORD_LEN, count = 0; i < wl->idx_sz; i++){
			// Each line will have as many words as possible separated by spaces
			// = width / (word_length + 1)
			words_per_line = td->wl_sz.c / (i + 1);
			words_of_len = wl->idx[i] - wl->idx[i - 1];
			tmp = count + DIV_ROUNDUP(words_of_len, words_per_line);
			if (tmp > td->wl_pos.r){
				// This is always lines 0 through n-1 of this word length
				// i.e. the last (possibly shortened) row isn't a problem since it will be mapped to 0 of the next word length
				*idx = i;
				*_wl = wl;
				return (td->wl_pos.r - count) * words_per_line;
			}
			count = tmp;
		}
		if (wl == &global_bonus){
			break;
		}
	}
	// TODO: error: td->wl_pos.r too large
	return -1;
}

void draw_score_bar(struct terminal_data* td){
	double progess;
	progress = (double)(global_score.score) / global_score.total_score * td->screen_sz.c;

}

void print_wl_word(struct terminal_data* td, struct word_data* wd, struct coord* word_pos, int start){
	WORD_BUFFER(buf);
	int i, j;
	if (!wd->found){
		obfuscate_word(buf, wd);
	}
	else {
		strcpy(buf, wd->word);
	}
	move_cursor(td->wl_pos.r + word_pos.r, td->wl_pos.c + word_pos.c);
	j = word_pos.c;
	if (start){
		printf("\xe2\x80\xa6"); // ellipsis
		start++;
		j++;
	}
	if (wd->uses_repeat){
		printf("\e[32m"); // green text
	}
	if (wd->uses_wildcard){
		printf("\e[4m"); // underline
	}
	for (i = start; j < td->wl_sz.c - 1; i++, j++){
		if (!buf[i]){
			break;
		}
		fputc(stdin, buf[i]);
	}
	if (!buf[i]){
		if (buf[i + 1]){
			printf("\xe2\x80\xa6"); // ellipsis
		}
		else {
			fputc(stdin, buf[i]);
		}
	}
	if (wd->uses_repeat || wd->uses_wildcard){
		printf("\e[0m");
	}
}

/*
Draw both global_wl and global_bonus at the side panel, starting at the current cursor position
	The word lists must be kept sorted by:
		length
			found/not found
				alphabetically
*/
void draw_wl(struct terminal_data* td){
	struct coord word_pos;
	struct word_list* wl;
	int idx, off, concat;
	off = get_wl_starting_idx(td, &idx, &wl);
	// TODO: print title
	word_pos.r = 0;
	for (;; wl = &global_bonus){
		// Iterate through word lengths
		for (; idx < wl->idx_sz; idx++){
			// Iterate through words of this length
			for (; off < wl->idx[idx]; word_pos.r++){
				if (word_pos.r >= td->wl_sz.r){
					break;
				}
				// Fit as many words in each line as possible
				word_pos.c = 1
				concat = max(td->wl_sz.c / (idx + 1), 1);
				for (; concat && off < wl->idx[idx]; concat--, off++) {
					print_wl_word(td, &wl->ls[wl->idx[idx - 1] + off], &word_pos, 0);
					word_pos.c += idx + 1;
				}
			}
		}
		if (wl == &global_bonus){
			break;
		}
	}
}

void move_wl_v(struct terminal_data* td, int n){
	// TODO: screen pos 1 to max
	int new = td->wl_cursor_pos + n;
	if (new < 0){
		if (td->wl_pos.r > 0){
			td->wl_pos.r += new;
			max_update(td->wl_pos.r, 0);
		}
		new = 0;
	}
	else if (new > td->screen_sz.r - 1){
		if (td->wl_pos.r < td->wl_sz.r - td->screen_sz.r - 1){
			td->wl_pos.r += new - (td->screen_sz.r - 1);
			min_update(td->wl_pos.r, td->wl_sz.r - td->screen_sz.r - 1);
		}
		new = td->screen_sz.r - 1;
	}
	td->wl_cursor_pos = new;
	draw_wl(td);
}

int terminal_send_word(struct terminal_data* td){
	int succ;
	int sc;
	// TODO: Make animation of tiles while building?
	// TODO: create tile list and/or verify it can be made
	succ = send_word(word, td->board, td.board_sz.c * td.board_sz.r);
	if (succ & WORD_FOUND){
		sc = score_word(word);
		if (succ & WORD_BONUS){
			global_score.bonus_score += sc
			global_score.bonus_found++;
		}
		else{
			global_score.score += sc;
			global_score.wl_found++;
		}
		
	}
}

// NOTE: need to add 1 to x when indexing board string because of newline

// The boards of the square tiles can intersect in multiple ways
// At a given intersection, there are four possible "limbs" that can meet up:
//   one from each cardinal direction (north, west, east, south)
#define TB_IU (1 << 0) // Up
#define TB_IL (1 << 1) // Left
#define TB_IR (1 << 2) // Right
#define TB_ID (1 << 3) // Down
const wchar_t TB_I = {
		/*
			Bits (paower of 2) assigned to each limb:
					1
				2	+	4
					8
		*/
	0x20,   // 0 space
	0x2579, // 1 up
	0x2578, // 2 left
	0x251b, // 3 up left
	0x257a, // 4 right
	0x2517, // 5 up right
	0x2501, // 6 left right
	0x253b, // 7 up left right
	0x257b, // 8 down
	0x2503, // 9 up down
	0x2513, // 10 left down
	0x252b, // 11 up left down
	0x250f, // 12 right down
	0x2523, // 13 up right down
	0x2533, // 14 left right down
	0x254b, // 15 up left right down
};

static unsigned char* TILE_FRAME;

// Sketch the basis for a tile in TILE_FRAME
static void setup_tile_frame(){
	int i;
	TILE_FRAME = calloc(TB_TILE_SZ_C * TB_TILE_SZ_R, sizeof(unsigned char));
	if (TILE_FRAME == NULL){
		// TODO: error
		return;
	}
	for (i = 0; i < TB_TILE_SZ_C; i++){
		if (i != 0){
			TILE_FRAME[i] |= TB_IR;
			TILE_FRAME[TB_TILE_SZ_C * (TB_TILE_SZ_R - 1) + i] |= TB_IR;
		}
		if (i != TB_TILE_SZ_C - 1){
			TILE_FRAME[i] |= TB_IL;
			TILE_FRAME[TB_TILE_SZ_C * (TB_TILE_SZ_R - 1) + i] |= TB_IL;
		}
	}
	for (i = 0; i < TB_TILE_SZ_R; i++){
		if (i != 0){
			TILE_FRAME[i * TB_TILE_SZ_C] |= TB_IU;
			TILE_FRAME[(i + 1) * TB_TILE_SZ_C - 1] |= TB_IU;
		}
		if (i != TB_TILE_SZ_R - 1){
			TILE_FRAME[i * TB_TILE_SZ_C] |= TB_ID;
			TILE_FRAME[(i + 1) * TB_TILE_SZ_C - 1] |= TB_ID;
		}
	}
}

// Add limb 'n' ([0, 3]) to intersect character 'curr' and return new character
wchar_t tb_iadd(wchar_t curr, char n){
	int i;
	for (i = 0; i < (1 << 4); i++){
		if (curr == TB_I[i]){
			return TB_I[i | n];
		}
	}
	// TODO: error: curr is invalid
	return 0;
}

void draw_tile(wchar_t* buffer, int nr, int nc, int r, int c){
	// nr = (max) number of tiles in row
	// nc = (max) number of tiles in col
	// r = starting row position in buffer (already scaled)
	// c = starting col position in buffer (already scaled)
	int pos, it, i, j;
	pos = (nc * TB_TILE_SZ_C + 2) * r + c;
	for (i = 0; i < TB_TILE_SIZE_Y; i++){
		for (j = 0; j < TB_TILE_SIZE_X; j++){
			it = pos + (nc * TB_TILE_SZ_C + 2) * i + j;
			buffer[it] = tb_iadd(buffer[it], TILE_FRAME[i * TB_TILE_SZ_C + j]);
		}
	}
}

// TODO: it might be a good idea to memoize this
wchar_t* create_board(const struct terminal_data* td){
	wchar_t* buffer;
	struct tile* t;
	long sz;
	int i, j;
	sz = (td->board_sz.c * TB_TILE_SZ_C + 2) * (td->board_sz.r * TB_TILE_SZ_R + 1);
	buffer = malloc((sz + 1) * sizeof(wchar_t));
	if (buffer == NULL){
		// TODO: error
		return NULL;
	}
	wmemset(buffer, ' ', sz);
	buffer[sz] = 0;
	t = td->board;
	for (i = 0; i < td->board_sz.r; i++){
		for (j = 0; j < td->board_sz.c; j++, t++){
			if (t->letter != ' '){
				draw_tile(buffer, td->board_sz.r, td->board_sz.c, i * TB_TILE_SIZE_Y, j * TB_TILE_SIZE_X);
			}
		}
	}
}

// Pad the board string with spaces so that it's rectangular (and thus indexable).
// The board array will be td->sz_x + 1 by td->sz_y. 'sz' is the length of 'str'.
unsigned char* pad_board(struct terminal_data* td, unsigned char* str, long sz){
	unsigned char* new;
	long i, j, k;
	new = malloc(((td->board_sz.c + 1) * td->board_sz.r + 1) * sizeof(unsigned char));
	if (new == NULL){
		// TODO: fail
	}
	for (i = j = k = 0; i < sz; i++, j++){
		if (str[i] == '\n'){
			// Pad rest of line with spaces
			k += td->board_sz.c;
			for (; j < k; j++){
				new[j] = ' ';
			}
			k++;
		}
		new[j] = str[i];
	}
	new[j] = 0;
	free(str);
	return new;
}

// Read the board file and return a string of the result. Perform checks to ensure it is valid.
// Empty lines are trimmed from the beginning and end.
// The longest line length is returned in td->board_sz.c; the number of lines is returned in td->board_sz.r.
// The return value must be freed
unsigned char* read_board_file(FILE* bf, struct terminal_data* td){
	// TODO: how to show in board file that tile can repeat?
	unsigned char* ret;
	long f_sz, i, j;
	unsigned char c;
	fseek(bf, -1, SEEK_END);
	f_sz = ftell(bf);
	if (f_sz < 0){
		// TODO: error
	}
	fseek(bf, 0, SEEK_SET);
	ret = malloc((f_sz + 2) * sizeof(unsigned char));
	if (ret == NULL){
		// TODO: error
	}
	if (fread(ret, f_sz, sizeof(unsigned char), bf) < f_sz){
		printf("Failed to read all of board file\n");
		// TODO
	}

	// Loop through characters, saving the number of newlines as sz.y
	// and the max gap between newlines as sz.x
	// First, trim off blank lines at the end
	for (i = f_sz - 1; i >= 0; i--){
		if (ret[i] != ' ' && ret[i] != '\n'){
			ret[i + 1] = '\n';
			ret[i + 2] = 0;
			break;
		}
	}
	if (i == 0){
		// TODO: error: blank file
	}
	f_sz = i + 2;
	// Next, trim off blank lines at the beginning
	for (j = 0, i = 0; i < f_sz; i++){
		if (ret[i] == '\n'){
			j = i + 1;
		}
		else if (ret[i] != ' '){
			break;
		}
	}
	// Shift non-blank lines down to beginning
	if (j > 0){
		for (i = j; i <= f_sz; i++){
			ret[i - j] = ret[i];
		}
	}
	f_sz -= j;

	// Now, count the number of lines and the length of the longest line
	td->board_sz.r = 0;
	td->board_sz.c = 0;
	// j is the number of characters in a line
	for (i = 0, j = 0; i < f_sz; i++){
		if (ret[i] == '\n'){
			td->board_sz.r++;
			if (j > td->board_sz.c){
				td->board_sz.c = j;
			}
			j = 0;
			continue;
		}
		else if (ret[i] != ' '){
			c = make_uppercase(&ret[i]);
			if (c < 0){
				printf("Unexpected character in board file: %c\n", ret[i]);
				// TODO
			}
			ret[i] = c;
		}
		j++;
	}
	if (td->board_sz.c > MAX_BOARD_SZ_C || td->board_sz.r > MAX_BOARD_SZ_R){
		// TODO: error
	}
	return pad_board(td, ret, f_sz);
}

void set_tile_neighbors(struct terminal_data* td, int r, int c){
	struct tile* neighbors[8];
	struct tile* t;
	int n, i, j;
	t = &td->board[r * td->board_sz.c + c];
	n = 0;
	if (t.letter != ' '){
		for (i = -1; i <= 1; i++){
			for (j = -1; j <= 1; j++){
				if (i != 0 || j != 0){
					if (r + i >= 0 && r + i < td->board_sz.r && c + j >= 0 && c + j < td->board_sz.c){
						neighbors[n] = &td->board[(r + i) * td->board_sz.c + (c + j)];
						if (neighbors[n].letter != ' '){
							n++;
						}
					}
				}
			}
		}
		t->tile_list = malloc(n * sizeof(struct tile*));
		if (t->tile_list == NULL){
			// TODO: error
		}
		memcpy(t->tile_list, neighbors, n * sizeof(struct tile*));
	}
	t->neighbors_len = n;
}

void reset_terminal(){
	if (terminal_set){
		printf("\e[m"); // reset color
    printf("\e[?25h"); // show cursor
    fflush(stdout);
		tcsetattr(0, TCSANOW, &old_termios);
		terminal_set = 0;
	}
}

void set_terminal(){
	if (!terminal_set){
		tcgetattr(0, &old_termios);
		memcpy(&termios, &old_termios, sizeof(termios));
    termios.c_lflag &= ~(ICANON | ECHO);
    termios.c_cc[VMIN] = 0;
    termios.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &termios);
    printf("\e[?25l"); // hide cursor
		terminal_set = 1;
    atexit(reset_terminal);
	}
}

void handler(int sig){
	loop = false;
}

int terminal_loop(struct terminal_data* td){
	int c;
	while (loop){
		switch ((c = fgetc(stdin))){
			case '\033': // Control char
				if ((c = fgetc(stdin)) != '['){
					break;
				}
				switch ((c = fgetc(stdin))){
					case 'A': // UP
					case 'B': // DOWN
						change_mode(td, MODE_WL);
						move_wl_v(t, (c - 'A') * 2 - 1); // 'A' -> -1; 'B' -> 1
					case 'C': // RIGHT
					case 'D': // LEFT
						if (td->mode == MODE_ENTRY){
							move_textbox(td, (c - 'C') * 2 - 1); // 'C' -> -1; 'D' -> 1
						}
						else if (td->mode == MODE_WL){
							move_wl_h(t, (c - 'C') * 2 - 1); // 'C' -> -1; 'D' -> 1
						}
				}
				break;
			case '\n':
				if (td->mode == MODE_ENTRY){
					terminal_send_word(td);
				}
				else {
					// TODO?
				}
				break;
			default:
				if (c >= 'a' && c <= 'z'){
					c -= 'a' - 'A';
				}
				else if (c < 'A' || c > 'Z'){
					continue;
				}
				change_mode(td, MODE_ENTRY);
				enter_letter(td, c);
		}
	}
}

int start(const struct word_file* wf, const unsigned char* board_file){
	struct terminal_data td;
	FILE* bf;
	unsigned char* board_str;
	int i, j;
	setlocale(LC_CTYPE, "");
	// Read in board file
	bf = fopen(board_file, "r");
	if (bf == NULL){
		// TODO: fail
	}
	board_str = read_board_file(bf, &td);
	if (board_str == NULL){
		// TODO error:
	}
	fclose(bf);
	td.board = malloc(td.board_sz.c * td.board_sz.r * sizeof(struct tile*));
	if (td.board == NULL){
		// TODO: fail
	}
	// Set all the tiles' letters
	for (i = 0; i < td.board_sz.r; i++){
		for (j = 0; j < td.board_sz.c; j++){
			td.board[i * td.board_sz.c + j].letter = board_str[i * (td.board_sz.c + 1) + j];
			// TODO: can_repeat
		}
	}
	// Set all the tiles' neighbors
	for (i = 0; i < td.board_sz.r; i++){
		for (j = 0; j < td.board_sz.c; j++){
			set_tile_neighbors(&td, i, j);
			// TODO: can_repeat
		}
	}
	if (!get_word_list(td.board, td.board_sz.c * td.board_sz.r, wf, &global_wl, &global_bonus)){
		// TODO: error
		goto out;
	}
	
	
	// TODO: draw
out:
	word_list_deinit(&global_wl);
	word_list_deinit(&global_bonus);
	free(td.board);
	free(board_str);
	return 0;
}
