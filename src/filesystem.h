#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <sys/ioctl.h>
#include <fuse.h>
#include <errno.h>


#define DEBUG 1

#define ALLOC(t,n) (t *) malloc((n)*sizeof(t))
#define CALLOC(t,n) (t *) calloc(sizeof(t),(n));

#define MAX_OPEN_FILES 100
#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H
#endif


// this file contains the settings used by the filesystem
#define BLOCK_SIZE 1024 //bytes
#define BGROUP_SIZE 1024		//no of blocks in a block group
#define FS_SIZE (100*1024 +1)*BLOCK_SIZE
#define DIRECT_BLOCKS 4
#define	INDIRECT_BLOCKS 4
#define	D_INDIRECT_BLOCKS 8
// #define T_INDIRECT_BLOCKS 1
#define NAME_SIZE 255
#define INODES 12800
#define FS_LABEL "myfs"

#define MAX_DEPTH 100
#define IROOT 0
#define FILESYTEM_PATH "testfile"
//enum {"READ_ONLY","READ_WRITE"};
//enum  { "FILE","DIR" };

typedef struct
{
	char name[NAME_SIZE];
	size_t size ;
  size_t free_inodes;
  size_t used_inodes;
  size_t free_blocks;
  size_t used_blocks;
	size_t bg_size;
  time_t creation,acess,change;
	unsigned short int block_size;
  char mount_mode;
}SUPERBLOCK;

typedef struct 
{
	char name[NAME_SIZE];
	size_t size;
	size_t d_blocks[DIRECT_BLOCKS];
	size_t i_blocks[INDIRECT_BLOCKS];
	size_t di_blocks[D_INDIRECT_BLOCKS];
  // size_t ti_blocks[T_INDIRECT_BLOCKS];
  time_t creation,acess,change;
  char type;
 // uuid_t uid,guid; 
}INODE;

typedef struct 
{
  unsigned int id;
  size_t free_inodes;
  size_t used_inodes;
  size_t free_blocks;
  size_t used_blocks;
  size_t imap_offset;
  size_t fmap_offset;
}GSBLOCK;

typedef struct 
{
  unsigned char length;
  size_t inode;
  size_t size;
  char type;
  char name[0];
}DIR_ENTRY;

typedef struct 
{
  uint32_t inode_no;
  INODE *inode;
  char mode; //r //w //b
  int count; 
}FILE_DESCRIPTOR;


int rootfd ;
FILE *logfile;
FILE_DESCRIPTOR **fdt;
// int goff;
// int gsize;
// char *gbuf;
// int gfd;
SUPERBLOCK *super_block;
//functions
int fs_format(char *file);
char ** parse_path(char *path,int * depth);
int diskwrite(int b_offset,void *data,int blocks);
int diskread(int b_offset,void *data,int blocks); 
FILE *log_open();
void log_msg(const char *format, ...);
void write_inode(uint32_t in,INODE *i_node);
INODE * get_inode(uint32_t in);
void free_block(uint32_t blk);
uint32_t get_block(INODE *fi,uint32_t i ,void *data);
int Fs_write (int fd, char *buf, int size, int offset,int flag);
int Fs_read (uint32_t fd, char *buf, int size, int offset);

