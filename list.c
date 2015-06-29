#include "list.h"

int lognum = 1;

static Node *AllocNode(void)
{
	return calloc(1, sizeof(Node));
}

static void SetNode(Node *n, const Log *x, Node *next)
{
	n->data = *x;
	n->next = next;
}

long logtime(void)
{
	time_t timer;
	long now;
	
	now = time(&timer);

	return now;
}

void log_op(Log *x , struct fuse_operations op)
{
	x->op = &op;
}

void Initialize(List *list)
{
	list->head = NULL;
	list->crnt = NULL;
}

void Insert(List *list, Log *x, struct fuse_operations op)
{
	x->num = lognum;
	lognum++;
	x->hour = logtime();
	log_op(x, op);

	Node *ptr = list->head;
	list->head = list->crnt = AllocNode();
	SetNode(list->head, x, ptr);
}
