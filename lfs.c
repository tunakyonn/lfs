/*
  Log-structured file system sample

  gcc -Wall lfs.c `pkg-config fuse --cflags --libs` -o lfs
*/

#include "lfs.h"

static List list;

//ログリスト初期化
void Log_init(Log_list *l)
{
	l->head = NULL;
	l->crnt = NULL;
}

//ログノードをalloc
Lnode *Log_AllocNode(void)
{
	return calloc(1, sizeof(Lnode));
}

void Log_SetNode(Lnode *ln, const Log_arg *y, Lnode *next)
{
	ln->arg = *y;
	ln->next = next;
}

//ログリストにノードをinsert
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

//fileリスト初期化
void list_init(List *list)
{
	list->head = NULL;
	list->crnt = NULL;
}

//fileノードをalloc
Node *AllocNode(void)
{
	return calloc(1, sizeof(Node));
}

void SetNode(Node *n, File_arg *x, Node *next)
{
	n->data = *x;
	n->next = next;
}

//fileリストにノードをinsert
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

//fileリストのノードを削除
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

//file systemのinitialization
void lfs_init(List *list)
{
	//最初にlfsファイルを作成
	Node *n;
	n = AllocNode();
	list_init(list);

	strcpy(n->data.f_name, "lfs");
	n->data.buf = NULL;
	n->data.size = 0;
	n->data.write_init = 0;
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

//get file name
char *get_filename(const char *path)
{
	char *p = (char*)malloc(127 * sizeof(char));
	 
    strcpy(p, path);
    p = p + 1;
    return p;
}

//ファイルのbufとsizeの変更
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

	return 0;
}

//ファイルのsize拡大
int lfs_expand(size_t new_size, Node *n)
{
	if (new_size > n->data.size)
		return lfs_resize(new_size, n);
	return 0;
}

//ファイルタイプの識別
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

//.getattr operation
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
				stbuf->st_nlink = 1;
				stbuf->st_size = n->data.size;

				char c[] = ".";
				char *ret;
				//スナップショットの場合
				if ((ret = strpbrk(p, c)) != NULL)
				{
					stbuf->st_mode = S_IFREG | 0444;
					return 0;
				}else{
					//普通のファイルの場合
					stbuf->st_mode = S_IFREG | 0644;

					//ログ取得
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

					return 0;
				}
			}
		}
	}
	return -ENOENT;
}

//.readdir operation
static int lfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
	(void) fi;
	(void) offset;

	//pathのルートじゃない場合エラー
	if (lfs_file_type(path) != LFS_ROOT)
		return -ENOENT;

	Node *n;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	for (n = list.head; n != NULL; n = n->next)
	{
		//リストにあるファイルの表示
		filler(buf, n->data.f_name, NULL, 0);

		//ログ取得
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
	return 0;
}

//.mknod operation
static int lfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	(void) mode;
	(void) rdev;

	if (lfs_file_type(path) == LFS_FILE)
		return -EEXIST;

	char *p = get_filename(path);
	Node *n = AllocNode();

	//ログ取得
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

	//ファイル作成
	strcpy(n->data.f_name, p);
	n->data.buf = NULL;
	n->data.size = 0;
	n->data.write_init = 0;
	Insert(&list, n);
	free(n);

	return 0;
}

//.unlink operation
static int lfs_unlink(const char *path)
{
	Node *n;
	char *p = get_filename(path);
	int init = 0;

	for (n = list.head; n != NULL; n = n->next)
	{
		if (strcmp(p, n->data.f_name) == 0){
			//ログ取得
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

			init += 1;
			break;
		}
	}

	//pathのファイルがない場合エラー
	if (init == 0)
		return -ENOENT;

	//ファイル削除
	Delete(&list, n);

	return 0;
}

//.truncate operation
static int lfs_truncate(const char *path, off_t size)
{
	//pathがファイルじゃない場合
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

	//pathのファイルがない場合エラー
	if (init == 0)
		return -ENOENT;

	//スナップショットの場合
	char c[2] = ".";
	char *ret;
	if ((ret = strpbrk(p, c)) != NULL)
	{
		fprintf(stderr, "スナップショットは書き込みできない\n");
		return -EACCES;
	}

	//ログ取得
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

	return lfs_resize(size, n);
}

//.utimens operation
static int lfs_utimens(const char *path, const struct timespec ts[2])
{
	(void) ts;

	//pathがファイルじゃない場合
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

	//pathのファイルがない場合エラー
	if (init == 0)
		return -ENOENT;

	//一回も書き込みない場合は0を返す
	if (n->data.write_init == 0)
		return 0;

	//ログ取得
	Lnode *ln;
	ln = Log_AllocNode();
	ln->arg.oper = uti;
	strcpy(ln->arg.path, path);
	ln->arg.stbuf = NULL;
	ln->arg.buf = NULL;
	ln->arg.size = 0;
	ln->arg.offset = 0;
	Log_insert(&n->l, ln);
	free(ln);

	Node *m = AllocNode();

	//現在時刻取得
	time_t timer;
	struct tm *t_st;
	time(&timer);
	t_st = localtime(&timer);
	char s1[6], s2[6], s3[6], s4[6], s5[6];
	sprintf(s1, ".%d:", t_st->tm_mon+1);
	sprintf(s2, "%d:", t_st->tm_mday);
	sprintf(s3, "%d:", t_st->tm_hour);
	sprintf(s4, "%d:", t_st->tm_min);
	sprintf(s5, "%d", t_st->tm_sec);

	char q[127] = ".";
	strcat(q, p);
	strcat(q, s1);
	strcat(q, s2);
	strcat(q, s3);
	strcat(q, s4);
	strcat(q, s5);
	printf("%s\n", q);

	//スナップショット作成
	strcpy(m->data.f_name, q);
	m->data.buf = (char *)calloc(n->data.size,sizeof(char));
	memcpy(m->data.buf, n->data.buf, n->data.size);
	m->data.size = n->data.size;
	Insert(&list, m);
	free(m);

	return 0;
}

//.open operation
static int lfs_open(const char *path, struct fuse_file_info *fi)
{
	(void) fi;

	//pathのファイル,ルートがない場合エラー
	if (lfs_file_type(path) == LFS_NONE)
		return -ENOENT;

	Node *n;
	char *p = get_filename(path);
	int init = 0;

	for (n = list.head; n != NULL; n = n->next)
	{
		if (strcmp(p, n->data.f_name) == 0)
		{
			//ログ取得
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
			init = 1;
			break;
		}
	}

	if (init == 0)
		return -ENOENT;

	//スナップショットの場合
	char c[] = ".";
	char *ret;
	if ((ret = strpbrk(p, c)) != NULL)
	{
		if (offset >= n->data.size)
			return 0;

		if (size > n->data.size - offset)
			size = n->data.size - offset;

		memcpy(buf, n->data.buf, size);
		fprintf(stderr, "Snap read\n");

		return size;
	}

	//普通のファイルの場合
	Lnode *ln;
	Node *s = AllocNode();
	s->data.buf = (char *)calloc(size,sizeof(char));
	lfs_resize(0, s);
	size = 0;

	for (ln = n->l.head; ln != NULL; ln = ln->next)
	{
		switch (ln->arg.oper) {
			case tru:
				lfs_resize(ln->arg.size, s);
				break;
			case wr:
				memcpy(s->data.buf + ln->arg.offset, ln->arg.buf, ln->arg.size);
				fprintf(stderr, "write size\n");
				printf("%d\n", (int)ln->arg.size);
				size += ln->arg.size;
				break;
			default:
				break;
		}
	}

	strcpy(buf, s->data.buf);
	free(s);
	fprintf(stderr, "Log read\n");

	//ログ取得
	Lnode *lln;
	lln = Log_AllocNode();
	lln->arg.buf = (char *)calloc(size,sizeof(char));
	lln->arg.oper = rd;
	strcpy(lln->arg.path, path);
	lln->arg.stbuf = NULL;
	memcpy(lln->arg.buf, buf, size);
	lln->arg.size = size;
	lln->arg.offset = offset;
	Log_insert(&n->l, lln);
	free(lln);

	return size;
}

//.read operation
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
			n->data.write_init = 1;
			init += 1;
			break;
		}
	}

	if (init == 0)
		return -ENOENT;

	//スナップショットの場合
	char c[2] = ".";
	char *ret;
	if ((ret = strpbrk(p, c)) != NULL)
	{
		fprintf(stderr, "スナップショットは書き込みできない\n");
		return -EACCES;
	}

	//ファイルのsize拡大
	if (lfs_expand(offset + size, n))
		return -ENOMEM;

	//ログ取得
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

	memcpy(n->data.buf + offset, buf, size);

	return size;
}

//.write operation
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
