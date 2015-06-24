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

// file operations
int lfs_resize(size_t new_size);

int lfs_expand(size_t new_size);

int lfs_file_type(const char *path);

static int lfs_getattr(const char *path, struct stat *stbuf);

static int lfs_utimens(const char *path, const struct timespec ts[2]);

static int lfs_open(const char *path, struct fuse_file_info *fi);

int lfs_do_read(char *buf, size_t size, off_t offset);

static int lfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

int lfs_do_write(const char *buf, size_t size, off_t offset);

static int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

static int lfs_truncate(const char *path, off_t size);

static int lfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);

#endif