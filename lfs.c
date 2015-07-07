/*
  Log-structured file system sample

  gcc -Wall lfs.c `pkg-config fuse --cflags --libs` -o lfs
*/

#include "lfs.h"

enum {
	LFS_NONE,
	LFS_ROOT,
	LFS_FILE,
};

static File_arg lfs;
static List list;
static Log_list log_list;

int lfs_initialized = 0;

void log_init(Log_list *log_list)
{
	log_list->head = NULL;
	log_list->crnt = NULL;
}


Lnode *Log_AllocNode(void)
{
	return calloc(1, sizeof(Lnode));
}

void Log_SetNode(Lnode *n, const Log_arg *y, Lnode *next)
{
	n->data = *y;
	n->next = next;
}

void Log_insert(Log_list *log_list, const Log_arg *y)
{
	Lnode *ptr = log_list->head;
	log_list->head = log_list->crnt = Log_AllocNode();
	Log_SetNode(log_list->head, y, ptr);
}

void list_init(List *list){
	list->head = NULL;
	list->crnt = NULL;
}

Node *AllocNode(void)
{
	return calloc(1, sizeof(Node));
}

void SetNode(Node *n, const File_arg *x, Node *next)
{
	n->data = *x;
	n->next = next;
}

void Insert(List *list, const File_arg *lfs)
{
	Node *ptr = list->head;
	
	while(ptr->next != NULL)
		ptr = ptr->next;
	ptr->next = list->crnt = AllocNode();
	SetNode(ptr->next, lfs, NULL);
}

void lfs_init()
{
	list_init(&list);

	Node *ptr = list.head;
	list.head = list.crnt = AllocNode();
	strcpy(lfs.f_name, "lfs");
	SetNode(list.head, &lfs, ptr);
	free(ptr);

	log_init(&log_list);
}

//get the file name from the path
char *get_filename(const char *path)
{
	char *p = (char*)malloc(sizeof(char) * 127);
	 
    strcpy(p, path);
    p = p + 1;
    return p;
}

int lfs_resize(size_t new_size)
{
	void *new_buf;

	if (new_size == lfs.size)
		return 0;

	new_buf = realloc(lfs.buf, new_size);
	if (!new_buf && new_size)
		return -ENOMEM;

	if (new_size > lfs.size)
		memset(new_buf + lfs.size, 0, new_size - lfs.size);

	lfs.buf = new_buf;
	lfs.size = new_size;

	return 0;
}

int lfs_expand(size_t new_size)
{
	if (new_size > lfs.size)
		return lfs_resize(new_size);
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
				stbuf->st_size = lfs.size;
				return 0;
			}
		}
	}
	return -ENOENT;
}

static int lfs_utimens(const char *path, const struct timespec ts[2])
{
	return 0;
}

static int lfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	(void) mode;
	(void) rdev;

	if (lfs_file_type(path) == LFS_FILE)
		return -EEXIST;

	char *p = get_filename(path);
	File_arg x;
	strcpy(x.f_name, p);
	Insert(&list, &x);	

	return 0;
}

static int lfs_open(const char *path, struct fuse_file_info *fi)
{
	if (lfs_file_type(path) == LFS_NONE)
		return -ENOENT;

	if (!lfs_initialized)
	{
		lfs_initialized = 1;
		lfs_resize(0);
	}
	return 0;
}

int lfs_do_read(char *buf, size_t size, off_t offset)
{
	if (offset >= lfs.size)
		return 0;

	if (size > lfs.size - offset)
		size = lfs.size - offset;

	memcpy(buf, lfs.buf + offset, size);

	return size;
}

static int lfs_read(const char *path, char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	(void) fi;

	if (lfs_file_type(path) != LFS_FILE)
		return -EINVAL;

	return lfs_do_read(buf, size, offset);
}

int lfs_do_write(const char *buf, size_t size, off_t offset)
{
	if (lfs_expand(offset + size))
		return -ENOMEM;

	memcpy(lfs.buf + offset, buf, size);

	return size;
}

static int lfs_write(const char *path, const char *buf, size_t size,
		      off_t offset, struct fuse_file_info *fi)
{
	(void) fi;

	if (lfs_file_type(path) != LFS_FILE)
		return -EINVAL;

	return lfs_do_write(buf, size, offset);
}

static int lfs_truncate(const char *path, off_t size)
{
	if (lfs_file_type(path) != LFS_FILE)
		return -EINVAL;

	return lfs_resize(size);
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
	}

	return 0;
}

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.mknod		= lfs_mknod,
	.truncate	= lfs_truncate,
	.utimens	= lfs_utimens,
	.open		= lfs_open,
	.read		= lfs_read,
	.write		= lfs_write,
};

int main(int argc, char *argv[])
{
	lfs_init();

	return fuse_main(argc, argv, &lfs_oper, NULL);
}
