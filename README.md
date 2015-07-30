# lfs
簡単なlog-structured filesystemもどきを作ろう

fuseで作ってます

## コンパイル方法

gcc -Wall lfs.c `pkg-config fuse --cflags --libs` -o lfs

## 実行方法

$mkdir /tmp/fuse

$./lfs /tmp/fuse

$cd /tmp/fuse

$ls

lfs

