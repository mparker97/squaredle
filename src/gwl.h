#ifndef GWL_H
#define GWL_H

#include "board.h"
#include "common.h"
#include "word_list.h"

bool get_word_list(struct tile* board, int len, struct word_file* wf, struct word_list* wl, struct word_list* bonus);

#endif
