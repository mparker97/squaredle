#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>

#define DIV_ROUNDUP(x, y) (((x) + (y) - 1) / (y))

#define POW2_ROUNDUP(x) \
	do { \
		(x)--; \
		(x) |= (x) >> 1; \
		(x) |= (x) >> 2; \
		(x) |= (x) >> 4; \
		(x) |= (x) >> 8; \
		(x) |= (x) >> 16; \
		(x)++; \
	} while (0)

#define min(x, y) (((x) < (y))? (x) : (y))
#define max(x, y) (((x) > (y))? (x) : (y))

// Update x to the minimum of x and y
#define min_update(x, y) \
	do { \
		if ((y) < (x)){ \
			(x) = (y); \
		} \
	} while (0)

// Update x to the maximum of x and y
#define max_update(x, y) \
	do { \
		if ((y) > (x)){ \
			(x) = (y); \
		} \
	} while (0)

#define bitmap_set(bm, bit) ((bm)[(bit) / 8] |= (1 << ((bit) % 8)))
#define bitmap_get(bm, bit) ((bm)[(bit) / 8] & (1 << ((bit) % 8)))

typedef unsigned char word_buffer_t[MAX_WORD_LEN + 1];

#define A_LIST_INIT_SZ 8

struct coord{
	int r;
	int c;
};

struct a_list{
	void** ls;
	int sz;
	int elm_sz;
};

int _a_list_init(struct a_list* l, int elm_sz){
	l->ls = malloc(A_LIST_INIT_SZ * elm_sz);
	if (l->ls == NULL){
		return 0;
	}
	l->sz = 0;
	l->elm_sz = elm_sz;
	return 1;
}

#define a_list_init(a, e) _a_list_init((struct a_list*)(a), e)

void _a_list_deinit(struct a_list* l){
	free(l->ls);
	l->ls = NULL;
}

#define ptr_a_list_deinit(a) _ptr_a_list_deinit((struct ptr_a_list*)(a))

void* _a_list_add(struct a_list* l){
	if (!(l->sz & (l->sz - 1)) && l->sz >= A_LIST_INIT_SZ){
		l->ls = realloc(l->ls, l->sz * 2 * l->elm_sz);
		if (l->ls == NULL){
			return NULL;
		}
	}
	return (void*)&(((char*)(l->ls))[l->sz++ * l->elm_sz]);
}

#define a_list_add(a) _a_list_add((struct a_list*)(a))

int _a_list_combine(struct a_list* a, struct a_list* b){
	// Assume a->elm_sz == b->elm_sz
	a->ls = realloc(a->ls, POW2_ROUNDUP(a->sz + b->sz) * b->elm_sz);
	if (a->ls == NULL){
		return 0;
	}
	memcpy((char*)(a->ls) + a->sz * a->elm_sz, b->ls, b->sz * b->elm_sz);
	a->sz += b->sz;
	_a_list_deinit(b);
	return 1;
}

#define a_list_combine(a, b) _a_list_combine((struct a_list*)(a), (struct a_list*)(b))

#endif
