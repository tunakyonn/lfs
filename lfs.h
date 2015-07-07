#ifndef LFS_H
#define LFS_H

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define MAXNAME 128

typedef struct {
	void		*buf;
	size_t		size;
	char		f_name[MAXNAME];
}File_arg;

typedef struct __node {
	File_arg		data;
	struct __node 	*next;
}Node;

typedef struct {
	Node *head;
	Node *crnt;
}List;


void list_init(List *list);
Node *AllocNode(void);
void SetNode(Node *n, const File_arg *x, Node *next);
void lfs_init();
char *get_filename(const char *path);
int lfs_resize(size_t new_size);
int lfs_expand(size_t new_size);
int lfs_file_type(const char *path);
int lfs_do_read(char *buf, size_t size, off_t offset);
int lfs_do_write(const char *buf, size_t size, off_t offset);


/* file_operations

static int lfs_getattr(const char *path, struct stat *stbuf);
static int lfs_utimens(const char *path, const struct timespec ts[2]);
static int lfs_mknod(const char *path, mode_t mode, dev_t rdev);
static int lfs_open(const char *path, struct fuse_file_info *fi);
static int lfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int lfs_truncate(const char *path, off_t size);
static int lfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
*/

#endif