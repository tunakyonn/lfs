/*
  Log-structured file system sample

  gcc -Wall lfs.c `pkg-config fuse --cflags --libs` -o lfs
*/

#include "lfs.h"

//#define LFS_NAME "lfs"

enum {
	LFS_NONE,
	LFS_ROOT,
	LFS_FILE,
};

static struct file_arg *lfs;
int lfs_initialized = 0;

void lfs_init()
{
	lfs = (struct file_arg*)malloc(sizeof(struct file_arg));
	strcpy(lfs->f_name, "lfs");
}

//get the file name from the path
char* get_filename(const char *path)
{
	char *p = NULL;
	char *q = NULL;

    strcpy(p, path);
    q = p + 1;

    return q;
}

int lfs_resize(size_t new_size)
{
	void *new_buf;

	if (new_size == lfs->size)
		return 0;

	new_buf = realloc(lfs->buf, new_size);
	if (!new_buf && new_size)
		return -ENOMEM;

	if (new_size > lfs->size)
		memset(new_buf + lfs->size, 0, new_size - lfs->size);

	lfs->buf = new_buf;
	lfs->size = new_size;

	return 0;
}

int lfs_expand(size_t new_size)
{
	if (new_size > lfs->size)
		return lfs_resize(new_size);
	return 0;
}

int lfs_file_type(const char *path)
{
	char file[] = "/";
	strcat(file, lfs->f_name);
	if (strcmp(path, "/") == 0)
		return LFS_ROOT;
	if (strcmp(path, file) == 0)
		return LFS_FILE;
	return LFS_NONE;
}

static int lfs_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = 0;

	switch (lfs_file_type(path)) {
	case LFS_ROOT:
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		break;
	case LFS_FILE:
		stbuf->st_mode = S_IFREG | 0644;
		stbuf->st_nlink = 1;
		stbuf->st_size = lfs->size;
		break;
	case LFS_NONE:
		return -ENOENT;
	}

	return 0;
}

static int lfs_utimens(const char *path, const struct timespec ts[2])
{
	return 0;
}

static int lfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
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
	if (offset >= lfs->size)
		return 0;

	if (size > lfs->size - offset)
		size = lfs->size - offset;

	memcpy(buf, lfs->buf + offset, size);

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

	memcpy(lfs->buf + offset, buf, size);

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
	filler(buf, lfs->f_name, NULL, 0);

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
