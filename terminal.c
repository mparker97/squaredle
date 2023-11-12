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

#include <stdlib.h>
#include <stdio.h>
#include <ioctl.h>
#include <wchar.h>
#include <locale.h>
#include "file.h"
#include "board.h"

#define MAX_BOARD_SZ_X 50
#define MAX_BOARD_SZ_Y 40

#define TB_TILE_SZ_X 5
#define TB_TILE_SZ_Y 4

struct terminal_data{
	struct word_list_final wlf;
	struct tile* board;
	int sz_x; // In tiles
	int sz_y; // In tiles
	int screen_sz_x;
	int screen_sz_y;
	int wl_scroll_pos;
	char textbox[MAX_WORD_LEN + 1];
	int textbox_pos;
	// bools
	char show_some_letters;
};

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
			Bits (power of 2) assigned to each limb:
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

static char* TILE_FRAME;

// Sketch the basis for a tile in TILE_FRAME
static void setup_tile_frame(){
	int i;
	TILE_FRAME = calloc(TB_TILE_SZ_X * TB_TILE_SZ_Y, sizeof(char));
	if (TILE_FRAME == NULL){
		// TODO: error
		return;
	}
	for (i = 0; i < TB_TILE_SZ_X; i++){
		if (i != 0){
			TILE_FRAME[i] |= TB_IR;
			TILE_FRAME[TB_TILE_SZ_X * (TB_TILE_SZ_Y - 1) + i] |= TB_IR;
		}
		if (i != TB_TILE_SZ_X - 1){
			TILE_FRAME[i] |= TB_IL;
			TILE_FRAME[TB_TILE_SZ_X * (TB_TILE_SZ_Y - 1) + i] |= TB_IL;
		}
	}
	for (i = 0; i < TB_TILE_SZ_Y; i++){
		if (i != 0){
			TILE_FRAME[i * TB_TILE_SZ_X] |= TB_IU;
			TILE_FRAME[(i + 1) * TB_TILE_SZ_X - 1] |= TB_IU;
		}
		if (i != TB_TILE_SZ_Y - 1){
			TILE_FRAME[i * TB_TILE_SZ_X] |= TB_ID;
			TILE_FRAME[(i + 1) * TB_TILE_SZ_X - 1] |= TB_ID;
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
	pos = (nc * TB_TILE_SZ_X + 2) * r + c;
	for (i = 0; i < TB_TILE_SIZE_Y; i++){
		for (j = 0; j < TB_TILE_SIZE_X; j++){
			it = pos + (nc * TB_TILE_SZ_X + 2) * i + j;
			buffer[it] = tb_iadd(buffer[it], TILE_FRAME[i * TB_TILE_SZ_X + j]);
		}
	}
}

// Draw the current segment of the word list on the screen
void draw_word_list(struct terminal_data* td){
	char buf[MAX_WORD_LEN + 1];
	struct wlf_idx* idx;
	int i, x, y, total;
	x = td->wl_x_low;
	y = td->wl_y_low;
	for (l = MIN_WORD_LEN; l < td->wlf.idx_sz; l++){
		idx = &td->wlf.idx[l];
		if (idx->idx == 0){
			continue;
		}
		total = idx->idx - (idx - 1)->idx;
		terminal_write_string(x, y, "%d letters (%d/%d):", l, idx->count, total);
		y++;
		for (i = 0; i < idx->count; i++){
			if (y >= td->wl_y_high){
				goto done;
			}
			terminal_write_string(x, y, td->wlf.wl.list[(idx - 1)->idx + i]);
			y++;
		}
		if (td->show_some_letters){
			for (; i < total; i++){
				if (y >= td->wl_y_high){
					goto done;
				}
				obfuscate_word(buf, td->wlf.wl.list[(idx - 1)->idx + i]);
				terminal_write_string(x, y, buf);
				y++;
			}
		}
	}
done:
}

// TODO: it might be a good idea to memoize this
wchar_t* td_create_board(const struct terminal_data* td){
	wchar_t* buffer;
	struct tile* t;
	long sz;
	int i, j;
	sz = (td->sz_x * TB_TILE_SZ_X + 2) * (td->sz_y * TB_TILE_SZ_Y + 1);
	buffer = malloc((sz + 1) * sizeof(wchar_t));
	if (buffer == NULL){
		// TODO: error
		return NULL;
	}
	wmemset(buffer, ' ', sz);
	buffer[sz] = 0;
	t = td->board;
	for (i = 0; i < td->sz_y; i++){
		for (j = 0; j < td->sz_x; j++, t++){
			if (t->letter != ' '){
				draw_tile(buffer, td->sz_y, td->sz_x, i * TB_TILE_SIZE_Y, j * TB_TILE_SIZE_X);
			}
		}
	}
}

// Pad the board string with spaces so that it's rectangular (and thus indexable).
// The board array will be td->sz_x + 1 by td->sz_y. 'sz' is the length of 'str'.
char* pad_board(struct terminal_data* td, char* str, long sz){
	char* new;
	long i, j, k;
	new = malloc(((td->sz_x + 1) * td->sz_y + 1) * sizeof(char));
	if (new == NULL){
		// TODO: fail
	}
	for (i = j = k = 0; i < sz; i++, j++){
		if (str[i] == '\n'){
			// Pad rest of line with spaces
			k += td->sz_x;
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
// The longest line length is returned in td->sz_x; the number of lines is returned in td->sz_y.
// The return value must be freed
char* read_board_file(FILE* bf, struct terminal_data* td){
	// TODO: how to show in board file that tile can repeat?
	char* ret;
	long sz, i;
	int j, sz_x, sz_y;
	char c;
	fseek(bf, -1, SEEK_END);
	sz = ftell(bf);
	if (sz < 0){
		// TODO: error
	}
	fseek(bf, 0, SEEK_SET);
	ret = malloc((sz + 2) * sizeof(char));
	if (ret == NULL){
		// TODO: error
	}
	if (fread(ret, sz, sizeof(char), bf) < sz){
		printf("Failed to read all of board file\n");
		// TODO
	}

	// Loop through characters, saving the number of newlines as sz_y
	// and the max gap between newlines as sz_x
	// First, trim off blank lines at the end
	for (i = sz - 1; i >= 0; i--){
		if (ret[i] != ' ' && ret[i] != '\n'){
			ret[i + 1] = '\n';
			ret[i + 2] = 0;
			break;
		}
	}
	if (i == 0){
		// TODO: error: blank file
	}
	sz = i + 2;
	// Next, trim off blank lines at the beginning
	for (j = 0, i = 0; i < sz; i++){
		if (ret[i] == '\n'){
			j = i + 1;
		}
		else if (ret[i] != ' '){
			break;
		}
	}
	// Shift non-blank lines down to beginning
	if (j > 0){
		for (i = j; i <= sz; i++){
			ret[i - j] = ret[i];
		}
	}
	sz -= j;

	// Now, count the number of lines and the length of the longest line
	td->sz_x = 0;
	td->sz_y = 0;
	// j is the number of characters in a line
	for (i = 0, j = 0; i < sz; i++){
		if (ret[i] == '\n'){
			td->sz_y++;
			if (j > td->sz_x){
				td->sz_x = j;
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
	if (td->sz_x > MAX_BOARD_SZ_X || td->sz_y > MAX_BOARD_SZ_Y){
		// TODO: error
	}
	return pad_board(td, ret, sz);
}

void set_tile_neighbors(struct terminal_data* td, int r, int c){
	struct tile* neighbors[8];
	struct tile* t;
	int n, i, j;
	t = &td->board[r * td->sz_x + c];
	n = 0;
	if (t.letter != ' '){
		for (i = -1; i <= 1; i++){
			for (j = -1; j <= 1; j++){
				if (i != 0 || j != 0){
					if (r + i >= 0 && r + i < td->sz_y && c + j >= 0 && c + j < td->sz_x){
						neighbors[n] = &td->board[(r + i) * td->sz_x + (c + j)];
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

int start(const struct word_file* wf, const char* board_file){
	struct terminal_data td;
	FILE* bf;
	char* board_str;
	struct word_list_final wlf;
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
	td.board = malloc(td.sz_x * td.sz_y * sizeof(struct tile*));
	if (td.board == NULL){
		// TODO: fail
	}
	// Set all the tiles' letters
	for (i = 0; i < td.sz_y; i++){
		for (j = 0; j < td.sz_x; j++){
			td.board[i * td.sz_x + j].letter = board_str[i * (td.sz_x + 1) + j];
			// TODO: can_repeat
		}
	}
	// Set all the tiles' neighbors
	for (i = 0; i < td.sz_y; i++){
		for (j = 0; j < td.sz_x; j++){
			set_tile_neighbors(&td, i, j);
			// TODO: can_repeat
		}
	}
	if (!get_word_list(td.board, td.sz_x * td.sz_y, wf, &wlf)){
		// TODO: error
		goto out;
	}
	
	
	// TODO: draw
out:
	word_list_deinit(&wl);
	free(td.board);
	free(board_str);
	return 0;
}
