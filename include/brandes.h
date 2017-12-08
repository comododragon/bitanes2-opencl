#ifndef BRANDES_H
#define BRANDES_H

typedef struct {
	int index;
	FP_T delta;
} deltapack_t;

#define LIST_MAX_SIZE 10000
#define LISTS_P_MAX_SIZE LIST_MAX_SIZE * LIST_MAX_SIZE

#define PIPE_DEPTH 1024

#endif
