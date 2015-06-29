#ifndef LIST_H
#define LIST_H

#include "lfs.h"

typedef struct{
	int 						num;
	long 						hour;
	struct fuse_operations 		*op;
	//char						*path;
}Log;

typedef struct __node {
	Log				data;
	struct __node 	*next;
}Node;

typedef struct {
	Node  *head;
	Node  *crnt;
}List;

long logtime(void);

void log_op(Log *x , struct fuse_operations op);

void Initialize(List *list);

void Insert(List *list, Log *x, struct fuse_operations op);

#endif