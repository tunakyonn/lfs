/*
  Log-structured file system sample

  gcc -Wall lfs.c `pkg-config fuse --cflags --libs` -o lfs
*/


#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define _XOPEN_SOURCE 700

#define LFS_NAME "lfs"

enum {
	LFS_NONE,
	LFS_ROOT,
	LFS_FILE,
};

static void *lfs_buf;
static size_t lfs_size;

static int lfs_resize(size_t new_size)
{
	void *new_buf;

	if (new_size == lfs_size)
		return 0;

	new_buf = realloc(lfs_buf, new_size);
	if (!new_buf && new_size)
		return -ENOMEM;

	if (new_size > lfs_size)
		memset(new_buf + lfs_size, 0, new_size - lfs_size);

	lfs_buf = new_buf;
	lfs_size = new_size;

	return 0;
}

static int lfs_expand(size_t new_size)
{
	if (new_size > lfs_size)
		return lfs_resize(new_size);
	return 0;
}

static int lfs_file_type(const char *path)
{
	if (strcmp(path, "/") == 0)
		return LFS_ROOT;
	if (strcmp(path, "/" LFS_NAME) == 0)
		return LFS_FILE;
	return LFS_NONE;
}

static int lfs_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

	switch (lfs_file_type(path)) {
	case LFS_ROOT:
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		//stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = 0;
		break;
	case LFS_FILE:
		stbuf->st_mode = S_IFREG | 0644;
		stbuf->st_nlink = 1;
		stbuf->st_size = lfs_size;
		//stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = 0;
		break;
	case LFS_NONE:
		return -ENOENT;
	}

	return 0;
}

static int lfs_open(const char *path, struct fuse_file_info *fi)
{
	if (lfs_file_type(path) == LFS_NONE)
		return -ENOENT;
	return 0;
}

static int lfs_do_read(char *buf, size_t size, off_t offset)
{
	if (offset >= lfs_size)
		return 0;

	if (size > lfs_size - offset)
		size = lfs_size - offset;

	memcpy(buf, lfs_buf + offset, size);

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

static int lfs_do_write(const char *buf, size_t size, off_t offset)
{
	if (lfs_expand(offset + size))
		return -ENOMEM;

	memcpy(lfs_buf + offset, buf, size);

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

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, LFS_NAME, NULL, 0);

	return 0;
}

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.truncate	= lfs_truncate,
	.open		= lfs_open,
	.read		= lfs_read,
	.write		= lfs_write,
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &lfs_oper, NULL);
}
