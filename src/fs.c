#include "filesystem.h"
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <string.h> 
#define IBS  (BLOCK_SIZE/4)  // indirect blocks


uint32_t min(uint32_t a,uint32_t b)
{
    if(a<b)return a;
    else return b;
}
uint32_t set_bit(uint32_t num,uint32_t index)
{
    return (1<<index )| num;
}
uint32_t clear_bit(uint32_t num,uint32_t index)
{
    return (~(1<<index ))& num;
}
int free_bit(uint32_t n)
{
    int i;
    for(i=31;i>=0;--i)
    {
        if(!((1<<i)&n))
            return i;
    }
}

// basic implementation of inode allocation
uint32_t alloc_inode(){
    //linear search 
    uint32_t inodes_per_group = INODES/((FS_SIZE/BLOCK_SIZE-1)/BGROUP_SIZE);
    char data[BLOCK_SIZE];
    uint32_t i,no_blk_grps = (FS_SIZE/1024 - 1)/BGROUP_SIZE; 
    for(i=0;i<no_blk_grps;++i)
    {
        diskread(i*BGROUP_SIZE + 1, data,1); //hardcode
        GSBLOCK *temp = (GSBLOCK *)data;
        // printf("INFO: looking for free block in %d group\n",i);

        if(temp->free_inodes>0)
        {
            // printf("INFO: free blk found in %d group\n",i);
            ++(temp->free_inodes);
            --(temp->used_inodes);            ;
            uint32_t *fmap = (uint32_t *)(data + temp->imap_offset);
            uint32_t j;
            for (j = 0; j < BGROUP_SIZE/32; ++j)
            {
                if(fmap[j]<(uint32_t)-1)
                    {
                        int index =free_bit(fmap[j]);
                        fmap[j]=set_bit(fmap[j],index);
                        diskwrite(i*BGROUP_SIZE + 1, data,1); //hardcode
                        ++super_block->used_inodes;
                        --super_block->free_inodes;
                        return i*inodes_per_group+j*32+(32-index)+1;
                    }
            }
        }
    }
    return -1;
    // ++i_count;
    // return i_count;
}
void free_inode(uint32_t i)
{
    if(i==0){  printf("Haha Nice try .You cant remove root");  exit(-1); }

    ++super_block->free_inodes;
    --super_block->used_inodes;
    uint32_t inodes_per_group = INODES/((FS_SIZE/BLOCK_SIZE-1)/BGROUP_SIZE);
    uint32_t group_no = (i-1)/inodes_per_group;
    uint32_t block_no = (i-1)%inodes_per_group;
    GSBLOCK gsb;
    char temp[BLOCK_SIZE];
                                            
    diskread(group_no+1,temp,1);
    
    ++(((GSBLOCK *)temp)->free_inodes);
    --(((GSBLOCK *)temp)->used_inodes);

    uint32_t *imap = (uint32_t *)(temp + ((GSBLOCK *)temp)->imap_offset);  //hardcode
    imap[block_no/32]=clear_bit(imap[block_no/32],32-(block_no%32));
    
    diskwrite(group_no+1,temp,1);
    INODE *inode = get_inode(i);
    int n = inode->size/BLOCK_SIZE+(inode->size%BLOCK_SIZE>0);
    for(i=0;i<n;++i)
    {
        free_block(get_block(inode,i,temp));
    }
    return;
}
void free_block(uint32_t blk)
{
    if(blk==0){  printf("Haha Nice try .You cant remove Super Block");  exit(-1); }
    uint32_t group_no = (blk-1)/BGROUP_SIZE;
    uint32_t block_no = (blk-1)%BGROUP_SIZE;
    GSBLOCK gsb;
    char temp[BLOCK_SIZE];
                                            
    diskread(group_no+1,temp,1);
    
    ++(((GSBLOCK *)temp)->free_blocks);
    --(((GSBLOCK *)temp)->used_blocks);

    uint32_t *fmap = (uint32_t *)(temp + ((GSBLOCK *)temp)->fmap_offset);  //hardcode
    fmap[block_no/32]=clear_bit(fmap[block_no/32],32-(block_no%32));
    
    diskwrite(group_no+1,temp,1);
    ++super_block->free_blocks;
    --super_block->free_blocks;
}

uint32_t find_free_block(INODE *parent, uint32_t i_parent,uint32_t no_blk  )
{
    //linear search 
    char data[BLOCK_SIZE];
    uint32_t i,no_blk_grps = (FS_SIZE/1024 - 1)/BGROUP_SIZE; 
    for(i=0;i<no_blk_grps;++i)
    {
        diskread(i*BGROUP_SIZE + 1, data,1); //hardcode
        GSBLOCK *temp = (GSBLOCK *)data;
        // printf("INFO: looking for free block in %d group\n",i);

        if(temp->free_blocks>0)
        {
            // printf("INFO: free blk found in %d group\n",i);
            ++(temp->free_blocks);
            --(temp->used_blocks);            ;
            uint32_t *fmap = (uint32_t *)(data + temp->fmap_offset);
            uint32_t j;
            for (j = 0; j < BGROUP_SIZE/32; ++j)
            {
                if(fmap[j]<(uint32_t)-1)
                    {
                        int index =free_bit(fmap[j]);
                        fmap[j]=set_bit(fmap[j],index);
                        diskwrite(i*BGROUP_SIZE + 1, data,1); //hardcode
                        ++super_block->used_blocks;
                        --super_block->free_blocks;
                        return i*BGROUP_SIZE+j*32+(32-index)+1;
                    }
            }
        }
    }
    return -1;
}

uint32_t alloc_blocks(INODE *file,uint32_t i_file,uint32_t no_blk)
{
    // printf("INFO: Allocating newblks\n");
    uint32_t new_blk = find_free_block(file,i_file,1);

    if(new_blk<= 0)
    {
        perror("Failed to allcoate a new block ");
        return -1;
    }
    uint32_t i = ceil((double)file->size/BLOCK_SIZE) ; 
    
    if(i < DIRECT_BLOCKS)
    {
        file->d_blocks[i]=new_blk;
        file->size += BLOCK_SIZE;
        // write_inode(i_file,file);
        return new_blk;
    }
    i = i- DIRECT_BLOCKS;
    

    uint32_t temp[BLOCK_SIZE/sizeof(uint32_t)];
    if(i<(INDIRECT_BLOCKS*IBS))
    {
        uint32_t ibn = i/IBS;
        int index_in_block = i%IBS;
        if(index_in_block == 0)
        {
            // printf("Indirect block case 1 , i = %d\n",i);

            uint32_t new_i_blk = find_free_block(file,i_file,1);
            if(new_i_blk<0)
            {
                free_block(new_blk);
                return -1;
            }
            file->i_blocks[ibn]=new_i_blk;
            temp[0]=new_blk;
            diskwrite(new_i_blk,temp,1);
            file->size += BLOCK_SIZE;
            // write_inode(i_file,file);
            return new_blk;
        }
        else
        {
            // printf("Indirect block case 2 , i = %d\n",i);
            diskread(file->i_blocks[ibn],temp,1);
            temp[index_in_block]=new_blk;
            diskwrite(file->i_blocks[ibn],temp,1);
            file->size += BLOCK_SIZE;
            // write_inode(i_file,file);
            return new_blk;
        }
    }
    i = i- INDIRECT_BLOCKS*IBS;
    if(i<D_INDIRECT_BLOCKS*IBS*IBS)
    {
        if(i%(IBS*IBS)== 0) // allocating 2 blocks for D_INDIRECT Block
        {   
            uint32_t new_i_blk1 = find_free_block(file,i_file,1);
            if(new_i_blk1<0)
            {
                free_block(new_blk);
                return -1;
            }
            uint32_t new_i_blk2 = find_free_block(file,i_file,1);
            if(new_i_blk2<0)
            {
                free_block(new_i_blk1);
                free_block(new_blk);
                return -1;
            }
            file->di_blocks[i/(IBS*IBS)]= new_i_blk1;
            uint32_t data[BLOCK_SIZE/sizeof(uint32_t)];
            data[0]=new_i_blk2;
            diskwrite(new_i_blk1,data,1);
            data[0]=new_blk;
            diskwrite(new_i_blk2,data,1);
            file->size += BLOCK_SIZE;
            // write_inode(i_file,file);
            return new_blk;
        }
        if(i%(IBS)==0) // allocating 1 block for Indirect block
        {
            uint32_t data[BLOCK_SIZE/sizeof(uint32_t)];
            uint32_t new_i_blk = find_free_block(file,i_file,1);
            if(new_i_blk<0)
            {
                free_block(new_blk);
                return -1;
            }
            diskread(file->di_blocks[i/(IBS*IBS)],data,1);
            data[i/IBS]=new_i_blk;
            diskwrite(file->di_blocks[i/(IBS*IBS)],data,1);
            data[0]=new_blk;
            diskwrite(new_i_blk,data,1); 

            file->size += BLOCK_SIZE;
            // write_inode(i_file,file);
            return new_blk;   
        }
        // no new block allocation required
        uint32_t data[BLOCK_SIZE/sizeof(uint32_t)];
        uint32_t data2[BLOCK_SIZE/sizeof(uint32_t)];
        diskread(file->di_blocks[i/(IBS*IBS)],data,1);
        diskread(data[i/IBS],data2,1);
        data2[i%IBS]=new_blk;
        diskwrite(data[i/IBS],data2,1);

        file->size += BLOCK_SIZE;
        // write_inode(i_file,file);
        return new_blk;   
    }
    return -1;
}

INODE * get_inode(uint32_t in)
{
    // printf("INFO: get inode\n");
    char data[BLOCK_SIZE];
    INODE * temp;
    if(in == 0)
    {
        diskread(0,(void *)data,1); // reading the whole block 
        temp = ALLOC(INODE,1);
        memcpy(temp,data+sizeof(SUPERBLOCK),sizeof(INODE));
        print_inode(temp,in);
        return temp;

    }
    uint32_t in_grp = INODES /(FS_SIZE/(BLOCK_SIZE*BGROUP_SIZE)) ;
    
    uint32_t gp = in/in_grp, pos = in %in_grp; // gn no and pos in group

    uint32_t boffset = gp*BGROUP_SIZE + pos*(BLOCK_SIZE/sizeof(INODE)) ;
    
    diskread(boffset,(void *)data,1); // reading the whole block 

    
    temp = ALLOC(INODE,1);
    int in_p_block = BLOCK_SIZE/sizeof(INODE); // inodes per block

    memcpy(temp,data+sizeof(INODE) * pos%in_p_block,sizeof(INODE));
    print_inode(temp,in);
    return temp;
}


void write_inode(uint32_t in,INODE *i_node)
{
    // printf("INFO:write_inode\n");
    
    char data[BLOCK_SIZE];
    INODE * temp;

    if(in == 0)
    {
        diskread(0,(void *)data,1); // reading the whole block 
        memcpy(data+sizeof(SUPERBLOCK),i_node,sizeof(INODE));
        diskwrite(0,data,1);
      
        return ;

    }
    uint32_t in_grp = INODES /(FS_SIZE/(BLOCK_SIZE*BGROUP_SIZE)) ;
    
    uint32_t gp = in/in_grp, pos = in %in_grp; // gn no and pos in group

    uint32_t boffset = gp*BGROUP_SIZE + pos*(BLOCK_SIZE/sizeof(INODE)) ;

    diskread(boffset,(void *)data,1); // reading the whole block 

    int in_p_block = BLOCK_SIZE/sizeof(INODE); // inodes per block

    memcpy(data+sizeof(INODE) * pos%in_p_block,i_node,sizeof(INODE));
    diskwrite(boffset,data,1);
}

uint32_t get_block(INODE *fi,uint32_t i ,void *data)
{
    // Need to implement some sort of caching to reduce repeatetive disk reads
    uint32_t size = ceil((double)fi->size/BLOCK_SIZE);

    uint32_t bdata[BLOCK_SIZE];
    if(i<DIRECT_BLOCKS && i<size)
    {
        diskread(fi->d_blocks[i],data,1);           
        return fi->d_blocks[i];
    }
    i = i-DIRECT_BLOCKS;
    size = size - DIRECT_BLOCKS  ;
   
    if(i<(INDIRECT_BLOCKS*IBS) &&  i<size)
    {
        diskread(fi->i_blocks[i/IBS],bdata,1);
        diskread(bdata[i%IBS],data,1);
        return bdata[i%IBS];
    }
    i = i - (INDIRECT_BLOCKS*IBS);
    size = size - INDIRECT_BLOCKS*IBS;

    if(i<(D_INDIRECT_BLOCKS*IBS*IBS) && i<size)
    {
        diskread(fi->di_blocks[i/(IBS*IBS)],bdata,1);
        uint32_t temp = bdata[i/IBS];
        diskread(bdata[temp],bdata,1);
        diskread(bdata[(i/IBS)%IBS],data,1);

        return bdata[(i/IBS)%IBS];
    }
    return -1;
}


int find_file_in_dir(char *filename,uint32_t p_inode, char *type)
{
    INODE * inode = get_inode(p_inode);
    uint32_t i,n = ceil((double)inode->size/BLOCK_SIZE);
    char data[BLOCK_SIZE];
    DIR_ENTRY *de;
    char name[NAME_SIZE];
    for(i=0;i<n;++i)
    {
        uint32_t blk = get_block(inode,i,data);
        uint32_t offset = 0;

        while(offset+sizeof(DIR_ENTRY)<=BLOCK_SIZE)
        {
            de = (DIR_ENTRY *)(data + offset);
            if(i==n-1 && offset+sizeof(DIR_ENTRY)> inode->size%BLOCK_SIZE) break;
            if(de->length== 0)break;
            strncpy(name,de->name,(uint32_t)de->length);
            name[de->length]='\0';
            if(strcmp(filename,name)==0)
            {
                *type = de->type;
                return de->inode;
            }
            offset += sizeof(DIR_ENTRY)+de->length;
        }
    }
    return -1;
}


int valid_file(char **path,int depth,int * flag,char *type)
{
    
    uint32_t i,parent=-1;
    int temp=IROOT;
    *type = 'd'; 
    for(i=0;i<depth;++i)
    {
        parent = temp;
        if(*type=='d')
        {
            temp=find_file_in_dir(path[i],temp,type);
        }
        else
            return -1;
        if(temp < 0)
            if(i == depth - 1)
                {
                    *flag = 0;

                    return parent;
                }
            else
                return -1;
    }
    if(i==depth)
    *flag = 1;
    return temp;
}


int write_directory_entry(INODE *parent,uint32_t p_inode,char *name,uint32_t inode,char type)
{
    log_msg("write_directory_entry %s\n",name);
    // printf("INFO: Writing DE %s,pinode %d,cinode %d,type %c\n",name,p_inode,inode,type);
    // printf("INFO : parent->size %d\n",parent->size);
    DIR_ENTRY *new_d_e;
    new_d_e = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY)+ strlen(name)+1);
    new_d_e->inode = inode;
    new_d_e->size = 0;
    new_d_e->type = type;
    new_d_e->length = (unsigned char)strlen(name);
    strcpy(new_d_e->name,name);
    // created dir entry

    int i = 0;
    char bdata[BLOCK_SIZE];
    if (parent->size%BLOCK_SIZE + sizeof(new_d_e) < BLOCK_SIZE && parent->size%BLOCK_SIZE!=0)
    {
        //when dir_entry doesnt need new block
        // printf("no new blk needed\n");
        uint32_t dblock = get_block(parent,parent->size/BLOCK_SIZE,bdata);
        memcpy(bdata+ (parent->size)%BLOCK_SIZE,new_d_e,sizeof(DIR_ENTRY)+strlen(name));
        diskwrite(dblock,bdata,1);
        parent->size+= sizeof(DIR_ENTRY)+strlen(name);
        // printf("PArent size %d\n",parent->size );
        write_inode(p_inode,parent);
        return 1;
    }
    // printf("new blk needed\n");
    // new block

    int dblock = alloc_blocks(parent,p_inode,1);
    if(dblock < 0)
    {
        // printf("ALloc block not working\n");
        return -1;
    }
    memcpy(bdata+ (parent->size)%BLOCK_SIZE,new_d_e,sizeof(DIR_ENTRY)+strlen(name));
    diskwrite(dblock,bdata,1);
    parent->size=parent->size-BLOCK_SIZE+ sizeof(DIR_ENTRY)+strlen(name);
    write_inode(p_inode,parent);
    
    return 1;
}
int fs_mknod(char *file_path, char mode)
{
    log_msg("INFO : Mknod file : %s\n",file_path);
    
    
    char **path,type;

    int depth,flag=-1;
    path = parse_path(file_path,&depth);
    
    INODE *stat ;
    int inode_no= valid_file(path,depth,&flag,&type);
    
    if(inode_no<0)
    {
        // printf("ERROR : Invalid file path\n");
        return -ENOTDIR;
    }
    if(flag==1) // file found
       { 
            // printf("File found\n");
            return -EEXIST; // eexist
        } //parent found
    else
    log_msg("INOF: Creating new file\n");
    
    stat=get_inode(inode_no);
    
    // populate inode 
    uint32_t i = alloc_inode();
    if(i<0)
    {
        // perror("No free inode\n");
        return -ENOSPC;
    }

    INODE *new = ALLOC(INODE,1);
    strcpy(new->name,path[depth-1]);
    new->size = 0 ;
    new->type = mode;
    time(&(new->creation));
    

    // put direectory entry in parent directory and return inode    
    if(write_directory_entry(stat,inode_no,path[depth-1],i,new->type)<0)
        {
            free_inode(i);
            return -EDQUOT;
        }


    write_inode(i,new);  
    return 0;
}
int check_fdtable(uint32_t inode_no,char mode)
{
    uint32_t i;
    for(i=0;i<MAX_OPEN_FILES;++i)
    {
        if(fdt[i]!=NULL && fdt[i]->inode_no == inode_no)
        {
            fdt[i]->count = fdt[i]->count+1;
            return i;
        }
    }
    for(i=0;i<MAX_OPEN_FILES;++i)
        if(fdt[i]==NULL)break;

    if(i==MAX_OPEN_FILES)return ENFILE;
    
    fdt[i]=ALLOC(FILE_DESCRIPTOR,1);
    fdt[i]->inode_no = inode_no ;
    fdt[i]->inode = get_inode(inode_no);
    
    fdt[i]->mode = mode ;
    fdt[i]->count = 1 ;
    return i;

}
int fs_open (char *file_path,char mode)
{
    log_msg("INFO : Opening file : %s ,mode %c\n",file_path,mode);

    char **path;
    char type ;
    int depth,flag;
    path = parse_path(file_path,&depth);

    INODE *stat ;
    int inode_no= valid_file(path,depth,&flag,&type);
    
    if(inode_no<0)
    {
        perror("ERROR : Invalid file path\n");
        return -ENOTDIR;
    }
    
    if(flag) // file found
       { 
            // printf("File found\n");
            return check_fdtable(inode_no,mode);
        } 
    else //parent found
        return -1; 
}

int fs_close(uint32_t fd)
{
    printf("INFO: closing fd %d, inode %d\n",fd,fdt[fd]->inode_no);
    if(fd >=MAX_OPEN_FILES || fd < 0)
        return -EBADF;
    fdt[fd]->count=fdt[fd]->count-1;
    if(fdt[fd]->count == 0)
    {
        fdt[fd]=NULL;
        printf("INFO removing file descriptor %d\n",fd);
    }
    return 0;
}
int simple_read(int fd, char *buf, int size, int offset)
{
    printf("INFO: Simpe read called \n");
    int fsize = fdt[fd]->inode->size;
    int i,n;
    i = offset/BLOCK_SIZE;
    n=min(offset+size,fsize)/BLOCK_SIZE;
    printf(" +++++++++++++++++++++++ N = %d\n",n); 
    for(;i<n;++i)
    {
        int blk = get_block(fdt[fd]->inode,i,buf+(i-offset/BLOCK_SIZE)*BLOCK_SIZE);
        printf("INFO : READING BLOCK %d\n",blk);
    }
    
    if(min(offset+size,fsize)%BLOCK_SIZE > 0)
    {
        char data[BLOCK_SIZE];
        int blk = get_block(fdt[fd]->inode,i,data);
        printf("INFO : READING BLOCK %d\n",blk);
        printf("get block %d and offset %d\n", i,BLOCK_SIZE *(i-offset/BLOCK_SIZE));
        strncpy(buf+ BLOCK_SIZE *(i-offset/BLOCK_SIZE),data,fsize%BLOCK_SIZE);
        
    }
    return min(fsize,offset+size) - offset;
}
int Fs_read (uint32_t fd, char *buf, int size, int offset)
{
//         offset = goff;
//         size = gsize;  
//         buf = gbuf;
//         fd = gfd ;
 
    printf("FS : read called fd %d ,size %d,offset %d \n",fd,size,offset); 
    if(fd>MAX_OPEN_FILES || fd < 0 || fdt[fd]==NULL)return -EBADF;
     
    INODE *file = fdt[fd]->inode;
    if(offset > file->size)return 0;

    if(offset%BLOCK_SIZE==0 && size %BLOCK_SIZE==0)
        return simple_read(fd,buf,size,offset);
    
    int ret;
    
    if(offset+size > file->size) 
        {
            size = file->size - offset;
        }
    ret = size;
    
    
    uint32_t i = offset/BLOCK_SIZE;
    offset = offset%BLOCK_SIZE;
    char data[BLOCK_SIZE];
    
    while(size>0)
    {   
        uint32_t blk = get_block(file,i,data);
        strncpy(buf,data+offset,min(BLOCK_SIZE-offset,size-offset));
        size = size - min(BLOCK_SIZE-offset,size-offset);
        buf = buf+ min(BLOCK_SIZE-offset,size-offset);
        offset = 0;
    }
    return ret;
}

int simple_write(int fd, char *buf, int size, int offset)
{
    printf("INFO: Simpe write called \n");
    INODE *file = fdt[fd]->inode;
    int fsize = file->size;

    // if(offset > fsize)      return -EINVAL;  
    
    int final_size=offset+size;

    int i,blkreq = (final_size)/BLOCK_SIZE+(final_size%BLOCK_SIZE > 0) - (fsize/BLOCK_SIZE + (fsize%BLOCK_SIZE > 0));
    printf("BLkreq %d\n",blkreq);
    if(blkreq>0)
    {
        int *newblks=ALLOC(uint32_t,blkreq);
        for(i=0;i<blkreq;++i)
        {
            newblks[i]=alloc_blocks(file,fdt[fd]->inode_no,1);
            printf("INFO : BLOCK ALLOCATED %d\n",newblks[i]);
            if(newblks[i]<0)
                break;
        }
        if(i!=blkreq) 
        {
            uint32_t j;
            for(j=0;j<i;++i)
                free_block(newblks[j]);
            file->size = file->size - i*BLOCK_SIZE;
            // write_inode(fdt[fd]->inode_no,file);
            free(newblks);
            return -ENOSPC;
        }
    }

    fdt[fd]->inode->size=offset+size;
    write_inode(fdt[fd]->inode_no,file);
    int n;
    i = offset/BLOCK_SIZE;
    n=(offset+size)/BLOCK_SIZE + ((offset+size)%BLOCK_SIZE > 0);
    
    char data[BLOCK_SIZE];
    printf("Warning content %d %d \n",i,n );
    for(;i<n;++i)
    {
        int blk = get_block(fdt[fd]->inode,i,data);
        diskwrite(blk,buf+(i-offset/BLOCK_SIZE)*BLOCK_SIZE,1);
        printf("INFO : WRITING BLOCK %d\n",blk);
    }

    return size;
}
int Fs_write (int fd, char *buf, int size, int offset,int flag)
{
    printf("FS : write called size %d, fd %d,offset %d \n",fd,size,offset); 
    if(fd>MAX_OPEN_FILES || fd < 0 || fdt[fd]==NULL)return -EBADF;
    
    if(offset%BLOCK_SIZE==0 )
        return simple_write(fd,buf,size,offset);
        
    
    INODE *file = fdt[fd]->inode;
    if(fdt[fd]->mode=='r' || offset > file->size)return -EINVAL;
    


    uint32_t i;
    
    if(offset+size > file->size)
    {
        uint32_t blkreq = ceil(((double)offset+size - ceil((double)file->size/BLOCK_SIZE)*BLOCK_SIZE)/BLOCK_SIZE);
        int *newblks=ALLOC(uint32_t,blkreq);
    
        for(i=0;i<blkreq;++i)
        {
            newblks[i]=alloc_blocks(file,fdt[fd]->inode_no,1);
            if(newblks[i]<0)
                break;
        }
        if(i!=blkreq) 
        {
            uint32_t j;
            for(j=0;j<i;++i)
                free_block(newblks[j]);
            file->size = file->size - i*BLOCK_SIZE;
            // write_inode(fdt[fd]->inode_no,file);
            free(newblks);
            return -ENOSPC;
        }
        free(newblks);
        file->size = offset+size; //updating filesize to accomodate the write
        write_inode(fdt[fd]->inode_no,file);
    }
    uint32_t ret = size;
    i = offset/BLOCK_SIZE;
    offset = offset%BLOCK_SIZE;
    char data[BLOCK_SIZE];
    while(size>0)
    {   
        uint32_t blk = get_block(file,i,data);
        strncpy(data+offset,buf,min(BLOCK_SIZE-offset,size-offset));
        size = size - min(BLOCK_SIZE-offset,size-offset);
        buf = buf+ min(BLOCK_SIZE-offset,size-offset);
        offset = 0;
        diskwrite(blk,data,1);
    }
    return ret;
}
void fs_read_dir(uint32_t  inode_no, void **buf, fuse_fill_dir_t filler)
{

    INODE * inode = get_inode(inode_no);
    // printf("INFOR READ DIR inode_no %d\n",inode_no);
    // printf("READ DIR size %d\n",inode->size);
    uint32_t i,n = ceil((double)inode->size/BLOCK_SIZE);
    char data[BLOCK_SIZE];
    DIR_ENTRY *de;
    char name[NAME_SIZE];
    // printf("READ DIR size %d\n",n);
    for(i=0;i<n;++i)
    {
        uint32_t blk = get_block(inode,i,data);
        uint32_t offset = 0;

        while(offset+sizeof(DIR_ENTRY)<=BLOCK_SIZE)
        {
            de = (DIR_ENTRY *)(data + offset);
            if(i==n-1 && offset+sizeof(DIR_ENTRY)> inode->size%BLOCK_SIZE) break;
            if(de->length== 0)break;
            strncpy(name,de->name,(uint32_t)de->length);

            name[(uint32_t)de->length]='\0';
            printf("%s\n",name);
            if(strcmp(name,"")!=0)
                filler(*buf,name,NULL,0);
            offset += sizeof(DIR_ENTRY)+de->length;
        }
    }
}
int findparent(char **path,int depth,int * flag,char *type)
{
    
    uint32_t i,parent=-1;
    int temp=IROOT;
    *type = 'd'; 
    for(i=0;i<depth;++i)
    {
        parent = temp;
        if(*type=='d')
        {
            temp=find_file_in_dir(path[i],temp,type);
            
        }
        else
            return -1;
        if(temp < 0)
            if(i == depth - 1)
                {
                    *flag = -1;

                    return parent;
                }
            else
                return -1;
    }
    if(i==depth)
    *flag = parent;
    return temp;
}
int remove_dir_entry(char *filename,uint32_t p_inode)
{
    INODE * inode = get_inode(p_inode);
    uint32_t i,n = ceil((double)inode->size/BLOCK_SIZE);
    char data[BLOCK_SIZE];
    DIR_ENTRY *de;
    char name[NAME_SIZE];
    for(i=0;i<n;++i)
    {
        uint32_t blk = get_block(inode,i,data);
        uint32_t offset = 0;

        while(offset+sizeof(DIR_ENTRY)<=BLOCK_SIZE)
        {
            de = (DIR_ENTRY *)(data + offset);
            if(i==n-1 && offset+sizeof(DIR_ENTRY)> inode->size%BLOCK_SIZE) break;
            if(de->length== 0)break;
            strncpy(name,de->name,(uint32_t)de->length);
            name[de->length]='\0';
            if(strcmp(filename,name)==0)
            {
                strcpy(de->name,"");
                diskwrite(blk,data,1);
                return de->inode;
            }
            offset += sizeof(DIR_ENTRY)+de->length;
        }
    }
    return -1;
}
int fs_unlink(char *file_path)
{
  if (strcmp("/",file_path)==0)
  {
    printf("cannot remove root\n");
    return -1;
  }
  
  char **path,type ;
  int depth,parent,inode_no;
  path = parse_path((char*)file_path,&depth);
  inode_no = findparent(path,depth,&parent,&type);
  
  if(inode_no < 0||parent <0)
  {
    printf("Cannot find the file to delete\n");
    return -1;
  }
   remove_dir_entry(path[depth-1],parent)  ;
  free_inode(inode_no);
  return 0;
}

int fs_mount(char *file)
{
    logfile = log_open();
    fdt = ALLOC(FILE_DESCRIPTOR *,MAX_OPEN_FILES);
    if(fdt == NULL)
    {
        perror("couldnt open fdt");
    }
    int i;
    for(i=0;i<MAX_OPEN_FILES;++i)
        fdt[i]=NULL;
    rootfd =  open(file,O_RDWR);
    if(rootfd<0)
        return -1;
    char *data = ALLOC(char,BLOCK_SIZE);
    diskread(0,data,1);
    super_block = (SUPERBLOCK *)data;
    return 1;
}
void fs_unmount()
{
    char data[BLOCK_SIZE];
    diskread(0,data,1);
    memcpy(data,super_block,sizeof(SUPERBLOCK));
    diskwrite(0,data,1);
    close(rootfd);
}