#include "filesystem.h"

static int hello_getattr(const char *file_path, struct stat *stbuf)
{
  int res = 0;
  memset(stbuf, 0, sizeof(struct stat));

  char **path,type ;
  int depth,flag,inode_no;
  if(strcmp("/","file_path")!=0)
  {
    path = parse_path((char*)file_path,&depth);
    inode_no = valid_file(path,depth,&flag,&type);
  }
  else
    inode_no = IROOT;

  if(inode_no == -1 || flag == 0)
    return -ENOENT;
  
  INODE * inode;
  inode = get_inode(inode_no);
  
  if(type == 'f')
    stbuf->st_mode = S_IFREG | 0666;

  if(type=='d')
    stbuf->st_mode = S_IFDIR | 0755;

  stbuf->st_nlink = 1;
  stbuf->st_size = inode->size; 
  stbuf->st_atime = inode->creation;
  stbuf->st_mtime = inode->creation;
  stbuf->st_ctime = inode->creation;

  
  stbuf->st_uid = 1000;
  stbuf->st_gid = 1000;
  // free(inode);
  return 0;
}

static int hello_readdir(const char *file_path, void *buf, fuse_fill_dir_t filler,
 off_t offset, struct fuse_file_info *fi)
{
 char **path,type ;
 int depth,flag,inode_no;
 if(strcmp("/","file_path")!=0)
 {
  path = parse_path((char*)file_path,&depth);
  inode_no = valid_file(path,depth,&flag,&type);
}
else
  inode_no = IROOT;
if(inode_no<0||type=='f')return -ENOENT;

filler(buf, ".", NULL, 0);
filler(buf, "..", NULL, 0);
fs_read_dir(inode_no, &buf, filler);
return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
  printf("INTERFACE: Open called %s path\n",path);
  int ret =0;
  if((fi->flags & 3) == O_RDWR)
  {
    printf("Onening in RW mode\n");
    ret = fs_open(path,'b');
    if(ret<0)return ret;
    fi->fh = ret;
    printf("INFO: File handler %d\n",ret);
    return 0;
  }
  
  if((fi->flags & 3 )== O_RDONLY)
  {
    printf("Onening in R mode\n");
    ret= fs_open(path,'r');
    if(ret<0)return ret;
    fi->fh = ret;
    printf("INFO: File handler %d\n",ret);
    return 0;
  }

  if((fi->flags & 3) == O_WRONLY)
  {
    printf("Onening in W mode\n");
    ret= fs_open(path,'w');
    if(ret<0)return ret;
    fi->fh = ret;
    printf("INFO: File handler %d\n",ret);
    return 0;

  }
  return -EACCES;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
  struct fuse_file_info *fi)
{
  // goff = (int)offset;
  // gsize = (int)size;
  // gbuf =(char *) buf;
  // gfd = fi->fh; 
  printf("INTERFACE: read called path %s,size %d,offset %d \n",path,(int)size,(int)offset);

  int ret = Fs_read (fi->fh,(char *)buf,(int)size,(int)offset);
  return ret;
}
static int hello_utimens (const char *path , const struct timespec tv[2])
{
  return 0;
}
static int hello_release(const char *path, struct fuse_file_info *fi)
{
  return fs_close(fi->fh);
}
static int hello_write (const char *path, const char *buf, size_t size, off_t offset,struct fuse_file_info *fi)
{
  // goff = (int)offset;
  // gsize = (int)size;
  // gbuf =(char *) buf; 
  // gfd = fi->fh;
  printf("INTERFACE: write called path %s,size %d,offset %d \n",path,size,(int)offset);
  int ret= Fs_write(fi->fh,(char *)buf,size,(int)offset,1);
  return ret;
}
static int hello_mknod (const char *path , mode_t mode, dev_t dev)
{
  return fs_mknod((char* )path,'f'); 
}
static int hello_mkdir(const  char  *path,mode_t mode)
{
 return fs_mknod(path,'d');
}


static int hello_rmdir(const  char  *path)
{
 int ret = fs_unlink(path);
 return 0;
}

static int hello_unlink(const  char  *path)
{
 int ret = fs_unlink(path);
 return 0;
}

static struct fuse_operations hello_oper = {
  .getattr   = hello_getattr,
  .readdir = hello_readdir,
  .open   = hello_open,
  .read   = hello_read,
  .rmdir  = hello_rmdir,
  .mkdir  = hello_mkdir,
  .unlink = hello_unlink,
  .mknod  = hello_mknod,
  .release  = hello_release,
  .utimens= hello_utimens,
  .write  = hello_write,
};

int main(int argc, char *argv[])
{
  if(strcmp("-f",argv[argc-1])==0)
  {
    if(fs_format(FILESYTEM_PATH)<0)
      printf("Cannot FOrmat\n");
      
    exit(0);
  }

  if(fs_mount(FILESYTEM_PATH)<0)
  {
    printf("Cannot mount\n");
    exit(-1);
  }

  return fuse_main(argc, argv, &hello_oper, NULL);
  fs_unmount();
}
