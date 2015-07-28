/*
  Log-structured file system sample

  gcc -Wall lfs.c `pkg-config fuse --cflags --libs` -o lfs
*/

#include "lfs.h"

static List list;
static int log_read = 0;

void Log_init(Log_list *l)
{
	l->head = NULL;
	l->crnt = NULL;
}

Lnode *Log_AllocNode(void)
{
	return calloc(1, sizeof(Lnode));
}

void Log_SetNode(Lnode *ln, const Log_arg *y, Lnode *next)
{
	ln->arg = *y;
	ln->next = next;
}

void Log_insert(Log_list *l, Lnode *ln)
{
	Lnode *lptr = l->head;

	if (l->head == NULL)
	{
		l->head = Log_AllocNode();
		Log_SetNode(l->head, &ln->arg, lptr);
	}else{
		while(lptr->next != NULL)
			lptr = lptr->next;
		lptr->next = Log_AllocNode();
		Log_SetNode(lptr->next, &ln->arg, NULL);
	}
}

void list_init(List *list)
{
	list->head = NULL;
	list->crnt = NULL;
}

Node *AllocNode(void)
{
	return calloc(1, sizeof(Node));
}

void SetNode(Node *n, File_arg *x, Node *next)
{
	n->data = *x;
	n->next = next;
}

void Insert(List *list, Node *n)
{
	Node *ptr = list->head;

	if (list->head == NULL)
	{
		list->head = AllocNode();
		SetNode(list->head, &n->data, ptr);
	}else{
		while(ptr->next != NULL)
			ptr = ptr->next;
		ptr->next = AllocNode();
		SetNode(ptr->next, &n->data, NULL);
	}
}

void Delete(List *list, Node *n)
{
	if (list->head != NULL)
	{
		if (list->head == n)
		{
			Node *ptr = list->head->next;
			free(n);
			list->head = ptr;
		}else{
			Node *t = list->head;
			while (t->next != n)
				t = t->next;
			t->next = n->next;
			list->crnt = t;
			free(n);
			list->crnt->next = t->next;
			return;
		}
	}
}

void lfs_init(List *list)
{
	Node *n;
	n = AllocNode();
	list_init(list);

	strcpy(n->data.f_name, "lfs");
	n->data.buf = NULL;
	n->data.size = 0;
	Insert(list, n);

	Lnode *ln;
	ln = Log_AllocNode();
	Log_init(&list->head->l);

	ln->arg.oper = mk;
	strcpy(ln->arg.path, "/lfs");
	ln->arg.stbuf = NULL;
	ln->arg.buf = NULL;
	ln->arg.size = 0;
	ln->arg.offset = 0;
	Log_insert(&list->head->l, ln);

	free(ln);
	free(n);
}

//get the file name from the path
char *get_filename(const char *path)
{
	char *p = (char*)malloc(127 * sizeof(char));
	 
    strcpy(p, path);
    p = p + 1;
    return p;
}

int lfs_resize(size_t new_size, Node *n)
{
	void *new_buf;

	if (new_size == n->data.size)
		return 0;

	new_buf = realloc(n->data.buf, new_size);
	if (!new_buf && new_size)
		return -ENOMEM;

	if (new_size > n->data.size)
		memset(new_buf + n->data.size, 0, new_size - n->data.size);

	n->data.buf = new_buf;
	n->data.size = new_size;

	//free(new_buf);

	return 0;
}

int lfs_expand(size_t new_size, Node *n)
{
	if (new_size > n->data.size)
		return lfs_resize(new_size, n);
	return 0;
}

int lfs_file_type(const char *path)
{
	Node *n;
	char *p = get_filename(path);

	if (strcmp(path, "/") == 0)
		return LFS_ROOT;
	for (n = list.head; n != NULL; n = n->next)
	{
		if (strcmp(p, n->data.f_name) == 0)
			return LFS_FILE;		
	}
	return LFS_NONE;
}

static int lfs_getattr(const char *path, struct stat *stbuf)
{	
	char *p = get_filename(path);
	Node *n;

	memset(stbuf, 0, sizeof(struct stat));
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();

	if (strcmp(path, "/") == 0){
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	} else {
		for (n = list.head; n != NULL; n = n->next)
		{
			if (strcmp(p, n->data.f_name) == 0){
				stbuf->st_mode = S_IFREG | 0644;
				stbuf->st_nlink = 1;
				stbuf->st_size = n->data.size;

				if (log_read == 0)
				{
					Lnode *ln;
					ln = Log_AllocNode();
					ln->arg.oper = ga;
					strcpy(ln->arg.path, path);
					ln->arg.stbuf = stbuf;
					ln->arg.buf = NULL;
					ln->arg.size = 0;
					ln->arg.offset = 0;
					Log_insert(&n->l, ln);
					free(ln);
				}
				return 0;
			}
		}
	}
	return -ENOENT;
}

static int lfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
	(void) fi;
	(void) offset;

	if (lfs_file_type(path) != LFS_ROOT)
		return -ENOENT;

	Node *n;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	for (n = list.head; n != NULL; n = n->next)
	{
		filler(buf, n->data.f_name, NULL, 0);
		if (log_read == 0)
		{
			Lnode *ln;
			ln = Log_AllocNode();
			ln->arg.oper = rdir;
			strcpy(ln->arg.path, path);
			ln->arg.stbuf = NULL;
			ln->arg.buf = 0;
			ln->arg.size = 0;
			ln->arg.offset = 0;
			Log_insert(&n->l, ln);
			free(ln);
		}
	}
	return 0;
}

static int lfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	(void) mode;
	(void) rdev;

	if (lfs_file_type(path) == LFS_FILE)
		return -EEXIST;

	char *p = get_filename(path);
	Node *n = AllocNode();

	if (log_read == 0)
	{
		Lnode *ln;
		ln = Log_AllocNode();
		ln->arg.oper = mk;
		strcpy(ln->arg.path, path);
		ln->arg.stbuf = NULL;
		ln->arg.buf = NULL;
		ln->arg.size = 0;
		ln->arg.offset = 0;
		Log_insert(&n->l, ln);
		free(ln);
	}

	strcpy(n->data.f_name, p);
	n->data.buf = NULL;
	n->data.size = 0;
	Insert(&list, n);

	free(n);

	return 0;
}

static int lfs_unlink(const char *path)
{
	Node *n;
	char *p = get_filename(path);
	int init = 0;

	for (n = list.head; n != NULL; n = n->next)
	{
		if (strcmp(p, n->data.f_name) == 0){
			if (log_read == 0)
			{
				Lnode *ln;
				ln = Log_AllocNode();
				ln->arg.oper = unln;
				strcpy(ln->arg.path, path);
				ln->arg.stbuf = NULL;
				ln->arg.buf = NULL;
				ln->arg.size = 0;
				ln->arg.offset = 0;
				Log_insert(&n->l, ln);
				free(ln);
			}
			init += 1;
			break;
		}
	}

	if (init == 0)
		return -ENOENT;

	strcpy(n->data.f_name, "");
	n->data.buf = NULL;
	n->data.size = 0;

	Delete(&list, n);

	return 0;
}

static int lfs_truncate(const char *path, off_t size)
{
	if (lfs_file_type(path) != LFS_FILE)
		return -EINVAL;

	Node *n;
	char *p = get_filename(path);
	int init = 0;

	for (n = list.head; n != NULL; n = n->next)
	{
		if (strcmp(p, n->data.f_name) == 0){
			init += 1;
			break;
		}
	}
	if (init == 0)
		return -ENOENT;

	if (log_read == 0)
	{
		Lnode *ln;
		ln = Log_AllocNode();
		ln->arg.oper = tru;
		strcpy(ln->arg.path, path);
		ln->arg.stbuf = NULL;
		ln->arg.buf = NULL;
		ln->arg.size = size;
		ln->arg.offset = 0;
		Log_insert(&n->l, ln);
		free(ln);
	}

	return lfs_resize(size, n);
}

static int lfs_utimens(const char *path, const struct timespec ts[2])
{
	return 0;
}

static int lfs_open(const char *path, struct fuse_file_info *fi)
{
	(void) fi;

	if (lfs_file_type(path) == LFS_NONE)
		return -ENOENT;

	Node *n;
	char *p = get_filename(path);
	int init = 0;

	for (n = list.head; n != NULL; n = n->next)
	{
		if (strcmp(p, n->data.f_name) == 0)
		{
			if (log_read == 0)
			{
				Lnode *ln;
				ln = Log_AllocNode();
				ln->arg.oper = op;
				strcpy(ln->arg.path, path);
				ln->arg.stbuf = NULL;
				ln->arg.buf = NULL;
				ln->arg.size = 0;
				ln->arg.offset = 0;
				Log_insert(&n->l, ln);
				free(ln);
			}
			init += 1;
			break;
		}
	}

	if (init == 0)
		return -ENOENT;

	return 0;
}

int lfs_do_read(const char *path, char *buf, size_t size, off_t offset)
{
	Node *n;
	char *p = get_filename(path);
	int init = 0;

	for (n = list.head; n != NULL; n = n->next)
	{
		if (strcmp(p, n->data.f_name) == 0){
			init += 1;
			break;
		}
	}

	if (init == 0)
		return -ENOENT;

	Lnode *ln;
	//log_read = 1;

	Node *s = AllocNode();
	s->data.buf = (char *)calloc(size,sizeof(char));
	lfs_resize(0, s);

	for (ln = n->l.head; ln != NULL; ln = ln->next)
	{
		switch (ln->arg.oper) {
			case tru:
				lfs_resize(ln->arg.size, s);
				break;
			case wr:
				fprintf(stderr, "CHECK3\n");
				memcpy(s->data.buf + ln->arg.offset, ln->arg.buf, ln->arg.size);
				printf("%d\n", (int)ln->arg.size);
				log_read++;
				break;
			default:
				break;
		}
	}

	if (log_read == 0)
	{
		size = 0;
		fprintf(stderr, "CHECK2\n");
		return size;
	}

	//memcpy(buf, s->data.buf + offset, size);
	strcpy(buf, s->data.buf);
	free(s);
	fprintf(stderr, "CHECK1\n");
	log_read = 0;

	return size;
}

static int lfs_read(const char *path, char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	(void) fi;

	if (lfs_file_type(path) != LFS_FILE)
		return -EINVAL;

	return lfs_do_read(path, buf, size, offset);
}

int lfs_do_write(const char *path, const char *buf, size_t size, off_t offset)
{
	Node *n;
	char *p = get_filename(path);
	int init = 0;

	for (n = list.head; n != NULL; n = n->next)
	{
		if (strcmp(p, n->data.f_name) == 0){
			init += 1;
			break;
		}
	}
	if (init == 0)
		return -ENOENT;

	if (lfs_expand(offset + size, n))
		return -ENOMEM;

	if (log_read == 0)
	{
		Lnode *ln;
		ln = Log_AllocNode();
		ln->arg.buf = (char *)calloc(size,sizeof(char));
		ln->arg.oper = wr;
		strcpy(ln->arg.path, path);
		ln->arg.stbuf = NULL;
		memcpy(ln->arg.buf, buf, size);
		ln->arg.size = size;
		ln->arg.offset = offset;
		Log_insert(&n->l, ln);
		free(ln);
	}

	memcpy(n->data.buf + offset, buf, size);

	return size;
}

static int lfs_write(const char *path, const char *buf, size_t size,
		      off_t offset, struct fuse_file_info *fi)
{
	(void) fi;

	if (lfs_file_type(path) != LFS_FILE)
		return -EINVAL;

	return lfs_do_write(path, buf, size, offset);
}

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.mknod		= lfs_mknod,
	.unlink		= lfs_unlink,
	.truncate	= lfs_truncate,
	.utimens	= lfs_utimens,
	.open		= lfs_open,
	.read		= lfs_read,
	.write		= lfs_write,
};

int main(int argc, char *argv[])
{
	lfs_init(&list);
	return fuse_main(argc, argv, &lfs_oper, NULL);
}
