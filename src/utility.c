#include "filesystem.h"

FILE *log_open()
{
    FILE *logfile;
    
    // very first thing, open up the logfile and mark that we got in
    // here.  If we can't open the logfile, we're dead.
    logfile = fopen("log", "w");
    if (logfile == NULL) {
    perror("logfile");
    exit(EXIT_FAILURE);
    }
    
    // set logfile to line buffering
    setvbuf(logfile, NULL, _IOLBF, 0);

    return logfile;
}
void print_inode(INODE *inode,uint32_t in)
{
//     printf("================Printing Inode %d ===============\n",in);
//     printf("name = %s\n",inode->name);
//     printf("Size = %d\n",inode->size);
//     printf("%c \n",inode->type);
//     printf("=========================\n");


}
void log_msg(const char *format, ...)
{

     if(!DEBUG)return;

    va_list ap;
    va_start(ap, format);
    // printf(format,ap );
    // vfprintf(logfile, format, ap);
}


char ** parse_path(char *path,int * depth)
{
	char **arr_path, *filepath;
	char *temp;
	
	if(strlen(path)==0)
		return NULL;
	
	filepath = ALLOC(char,strlen(path)+1);
	filepath[strlen(path)]='\0';
	strcpy(filepath,path);

	arr_path = ALLOC(char *, sizeof(MAX_DEPTH));
	
	temp = (char *)strtok(filepath,"/");
	int i=0;
	while(temp != NULL)
	{
		arr_path[i] = temp;
		temp = (char *)strtok (NULL,"/");
		++i;
	}
	*depth =i ;
    // printf("parse path\n");
    for(i=0;i<*depth;++i)
        printf("%s\n",arr_path[i]);
    // printf("Depth %d \n", *depth);
	return arr_path;
}

int diskwrite(int b_offset,void *data,int blocks)
{
    lseek(rootfd,BLOCK_SIZE * b_offset,SEEK_SET);
    write(rootfd,data,BLOCK_SIZE*blocks);
}

int diskread(int b_offset,void *data,int blocks)
{
    lseek(rootfd,BLOCK_SIZE * b_offset,SEEK_SET);
    read(rootfd,data,blocks * BLOCK_SIZE);
}

int write_group(size_t id)
{
    char data [BLOCK_SIZE];
    int i;
    GSBLOCK gsb;
    
    // writing the group sblock
    gsb.id = id;
    gsb.free_inodes = 129;  // hardcode
    gsb.used_inodes = 0;
    gsb.free_blocks = 1024 - 1 -(129/3); // hardcode
    gsb.fmap_offset = sizeof(gsb);
    gsb.imap_offset = gsb.fmap_offset + BGROUP_SIZE/8;
    memcpy(data,&gsb,sizeof(gsb));

    unsigned int *fmap = (unsigned int *)(data +gsb.fmap_offset);
    //writing the fmap 128 bytes
    for (i=0;i<128/4;++i)
    {
        fmap[i]=0x00000000;
        if (i==0)
            fmap[i]=0xffffffff;
        if (i==1)
            fmap[i]=0xfff00000;
    }
        
    //writing the imap 
    unsigned int *imap = (unsigned int *)(data +gsb.imap_offset);
    for(i=0;i<128/32;++i)
        imap[i]=0x00000000;

    diskwrite(id * BGROUP_SIZE +1 ,(void *)data,1);
    memset((void *)data,0,BLOCK_SIZE);

    // writing other blocks
    for(i=1;i<BLOCK_SIZE-1;++i)
        diskwrite(id * BGROUP_SIZE +1+i,(void *)data,1);

}

int fs_format (char *file)
{
    
    log_msg("INFO : Formating %s\n",file);
    rootfd = open(file,O_RDWR|O_CREAT,0644);
    if (rootfd <0){
        perror("Unable to open file ");
        return -1;
    }
    
    // int numblocks;
    // ioctl(rootfd, 4704, &numblocks);     // blkgetsize 4704
    int n =  FS_SIZE / BLOCK_SIZE;
    // int n = numblocks/(BLOCK_SIZE/512);
 

    printf("Size of Superblock %d\n",sizeof(SUPERBLOCK));
    printf("Size of Inode %d\n",sizeof(INODE));
    printf("Size of Gsblock %d\n",sizeof(GSBLOCK));
    printf("Size of dir entry %d\n",sizeof(DIR_ENTRY));
    printf("Blocks = %d \n",n);

    int bgn = ceil((n-1)/ BGROUP_SIZE);
    printf("No_of bgs %d \n",bgn);
    
    int n_i_g = ceil(INODES/bgn);
    
    printf("No of inodes per group %d\n",n_i_g);
    
    int n_b_g = ceil((n-1)/bgn);
    printf("no of blocks per group %d\n", n_b_g);
       //write superblock

    char data[BLOCK_SIZE];
    SUPERBLOCK sb;
    strncpy ((char  *)&sb.name,FS_LABEL,NAME_SIZE);
    sb.free_inodes = INODES-1;
    sb.used_inodes = 1;
    sb.free_blocks = n -bgn *44 -1;
    sb.used_blocks = bgn *44 +1;
    sb.bg_size =  BGROUP_SIZE;
    sb.mount_mode = 'u';
    sb.block_size = BLOCK_SIZE;
    memcpy(data,&sb,sizeof(SUPERBLOCK));
    
    //write 0th inode 
    INODE root;
    strcpy(root.name,"root");
    root.size = 0;
    root.type = 'd';
    memcpy((char *)data + sizeof(SUPERBLOCK),&root,sizeof(INODE));
    diskwrite(0,data,1);
   
    INODE check;
    diskread(0,data,1);
    memcpy(&check,data+sizeof(SUPERBLOCK),sizeof(INODE));
    print_inode(&check,0);
    // write block groups
    int i ;
    for (i=0;i<=(n-1)/BGROUP_SIZE;++i)
            write_group(i);
    close(rootfd);
    log_msg("INFO : Format Complete\n");
    return 1;
}
