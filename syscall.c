#include "all_include.h"
// #include "disk.h"
// #include "write_to_log.h"
// #include "syscall.h"
// #include "initialise.h"
// #include "dir.h"
// #include "file.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>


/*
syscall_format 
	- Creates a new filesystem on the disk, destroying any data already present
	- Sets aside ten percent of the blocks for inodes 
	- clears the inode table
	- writes the superblock 
	- Returns one on success, zero otherwise. Note that formatting a filesystem does not cause it to be mounted. Also, an attempt to format an already-mounted disk should do nothing and return failure. */
int syscall_format(int reset)
{
	
	ResetLogFile();
	LogWrite("System Format\n");
	
	//initialise disk file
	if(disk_init("memory_file.dat",NUMBER_OF_BLOCKS)!=1){
		LogWrite("Disk init failed");
		return 0;
	}
	//initialise filetable
	syscall_initial_filetable();

	//initialise superblock
	int superret=init_superblock();
	if(superret!=1) return 0;

	//view superblock details
	syscall_debug();

	//initialise empty inodes
	int inoderet=initialise_empty_inodes(reset);
	if(inoderet!=1) return 0;

	int init_filetable=syscall_initial_filetable();
	if(init_filetable!=1) return 0;
	return 1;
}

/*
syscall_debug 
	-Report on how the inodes and blocks are organized. 
*/
void syscall_debug()
{
	union syscall_block block;

	disk_read(0,block.data);

	printf("\nsuperblock:\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);
}

/*
syscall_mount 
	- Examine the disk for a filesystem
	- read the superblock
	- build a free block bitmap
		(Each time that an SimpleFS filesystem is mounted, the system must build a new free block bitmap from scratch by scanning through all of the inodes and recording which blocks are in use)
	- prepare the filesystem for use
	-Return one on success, zero otherwise
*/
int syscall_mount()
{
	union syscall_block block;
	disk_read(0,block.data);
	if(block.super.magic!=DISK_MAGIC){
		LogWrite("Disk not correct\n");
		LogWrite("Syscall Mount Failed\n");
		return 0;
	}
	LogWrite("Disk is valid\n");

	int k=0; //for every actual inode
	struct syscall_inode Inode;

	for(int i=1;i<=NUMBER_OF_INODE_BLOCKS;i++){ //for every inode block
		disk_read(i,block.data);
		for(int j=0;j<INODES_PER_BLOCK;j++){//for every inode in the block
			Inode = block.inode[j];
			//printf("\ninode %d pointers %d\n",k,POINTERS_PER_INODE);
			/*
			for every data block pointer in the inode
				check if the pointer exists
				if it does, mark that data block in the free block bitmap
			*/
			for(int l=0;l<POINTERS_PER_INODE;l++){
				//printf("%d ",Inode.direct[l]);
				if(Inode.direct[l]!=-1){
					free_block_bitmap[Inode.direct[l]]=1;
				}
			}
			k++;
		}
	}

	LogWrite("Free Block Bitmap created\n");
	return 1;
}


/*
Given inode number, assigns 4 datablocks to every inode as part of its datablocks
*/
int syscall_assign_datablocks(int inode_num){
	LogWrite("Assigning datablocks to inode\n");
	for(int x = 0; x<POINTERS_PER_INODE ;x++){
		int free_datablock_num = syscall_find_free_datablock();
		//printf("free_datablock_num %d \t", free_datablock_num);
		if(free_datablock_num != -1){
			free_block_bitmap[free_datablock_num] = 1;
			i_list[inode_num].direct[x] = free_datablock_num;
		}
		else{
			return -1;
		}
	}
	write_i_list_to_disk();
	syscall_display_datablock_for_inode(inode_num);
	//printf("\n");
	return 1;
}

void syscall_display_datablock_for_inode(int inodenum){

	struct syscall_inode Inode=ReadInode(inodenum);
	printf("\nINODE %d\npointer\tdatablock",inodenum);
	for(int i=0;i<POINTERS_PER_INODE;i++){
		printf("\n  %d\t%d",i,Inode.direct[i]);
	}
}

void syscall_free_datablock_for_inode(int inodenum){

	struct syscall_inode Inode=ReadInode(inodenum);
	printf("\nINODE %d\npointer\tdatablock",inodenum);
	for(int i=1;i<POINTERS_PER_INODE;i++){
		Inode.direct[i]=-1;
	}
	i_list[inodenum]=Inode;
	write_i_list_to_disk();

}

/*
syscall_create 
	- Create a new inode of zero length
	- On success, return the (positive) inumber
	- On failure, return -1
*/
int syscall_create_Inode()
{
	int i;
	union syscall_block block;
	for(i=0; i<NUMBER_OF_INODES; i++){
		//Find free inode
		if(i_list[i].isvalid == 0){

			//Initialize		
			i_list[i].isvalid = 1;
			i_list[i].size = 0;

			//Set time of creation
			//clock_gettime(CLOCK_REALTIME, &i_list[i].i_ctime);


			//Get disk information 
			int blocknumber = calculate_block_for_inode(i);
			int block_offset = calculate_offset_in_block(i,blocknumber);

			//Read block with inode from disk 
			//printf("Reading block %d ... \n", blocknumber);
			disk_read(blocknumber, block.data);

			//Update inode information			
			i_list[i].blocknum = blocknumber;


			//assign 4 datablocks (to be filled by file_create/dir_create functions)
			int x = syscall_assign_datablocks(i);
			if(x < 0){
				i_list[i].isvalid = 0;
				LogWrite("No free datablocks! Cannot store file information\n");
				return -1;
			}

			//Update block with new inode information
			block.inode[block_offset] = i_list[i];

			//Write inode block back to disk
			disk_write(blocknumber, block.data);



			//Log creation
			//printf("Created inode %d in block %d\n", i, i_list[i].blocknum);
			LogWrite("Created inode successfully\n");

			return i;
		}
	}
	LogWrite("No free inodes! Out of space \n");
	return -1;
}


/*
Find an available free datablock from free_block_bitmap
*/
int syscall_find_free_datablock(){

	//printf("datablock_start %d", DATABLOCK_START);
	for(int i=DATABLOCK_START; i<NUMBER_OF_BLOCKS; i++){
		if(free_block_bitmap[i] == 0){
			free_block_bitmap[i] = -1;
			return i;
		}
	}
	return -1;

}

/*
	Initialises stat file info like ctim,inode num, hardlink count in block 0
*/
int syscall_initialise_file_info(int inode_num, int file_type){

	//Read the block containing the inode information
	struct syscall_inode Inode = ReadInode(inode_num);
	int stat_block_num = Inode.direct[0];

	//Read the stat block from inode
	union syscall_block block;
	disk_read(stat_block_num, &block);

	//Set inodeNumber, number of hard links, inode status change time na number of blocks it occupies
	//struct fs_stat stat_buf;
	block.stat_info.st_mode = file_type;
	block.stat_info.st_ino = inode_num;
	//stat_buf.st_uid ;
	//stat_buf.st_gid ;
	//stat_buf.st_size ;
	//stat_buf.st_atim ;
	//stat_buf.st_mtim ;
	clock_gettime(CLOCK_REALTIME, &block.stat_info.st_ctim);
	block.stat_info.st_blksize=DISK_BLOCK_SIZE;
	block.stat_info.st_blocks = POINTERS_PER_INODE;

	if(file_type == S_IFDIR){
		block.stat_info.st_nlink = 2;
	}
	else{
		block.stat_info.st_nlink = 1;
	}

	//printf("\nInitialized inode %d with stat information as follows : \n st_ino %d \nst_nlink %d \nst_blocks %d \n",
		//inode_num, block.stat_info.st_ino, block.stat_info.st_nlink, block.stat_info.st_blocks);
	syscall_display_stat(inode_num);
	disk_write(stat_block_num, &block);
	
	//LogWrite("Initialized stat file info\n");
	return 1;

}

  
void syscall_display_stat(int inodenum){
	//Read the block containing the inode information
	struct syscall_inode Inode = ReadInode(inodenum);
	int stat_block_num = Inode.direct[0];

	//Read the stat block from inode
	union syscall_block block;
	disk_read(stat_block_num, &block);
	
	//Stat file information structure
	printf("\nStat for inode %d",inodenum);
	printf("\n\tst_mode   : %d",block.stat_info.st_mode);
	printf("\n\tst_ino    : %d",block.stat_info.st_ino);
	printf("\n\tst_dev    : %d",block.stat_info.st_dev);
	printf("\n\tst_rdev   : %d",block.stat_info.st_rdev);
	printf("\n\tst_nlink  : %d",block.stat_info.st_nlink);
	printf("\n\tst_uid    : %d",block.stat_info.st_uid);
	printf("\n\tst_gid    : %d",block.stat_info.st_gid);
	printf("\n\tst_size   : %d",block.stat_info.st_size);
	printf("\n\tst_blksize: %d",block.stat_info.st_blksize);
	printf("\n\tst_blocks : %d",block.stat_info.st_blocks);
	printf("\n\tst_atim   : %lld.%.9ld", (long long)block.stat_info.st_atim.tv_sec, block.stat_info.st_atim.tv_nsec);
	printf("\n\tst_st_mtim: %lld.%.9ld", (long long)block.stat_info.st_mtim.tv_sec, block.stat_info.st_mtim.tv_nsec);
	printf("\n\tst_ctim   : %lld.%.9ld", (long long)block.stat_info.st_ctim.tv_sec, block.stat_info.st_ctim.tv_nsec);
	printf("\n");
	return 0;
}

 struct stat syscall_lstat(int inodenum)
{
	struct syscall_inode Inode = ReadInode(inodenum);
	int stat_block_num = Inode.direct[0];

	//Read the stat block from inode
	union syscall_block block;
	disk_read(stat_block_num, &block);
	struct stat buf;
	buf=block.stat_info;
	return buf;
}
/*
	Creates the default directories . and .. 
*/
int syscall_create_default_dir(int parent_inode, int new_inode){
  
	//Keeps track of number of directory entries
	int num_dirents = 0;

	//Read the block containing the inode information
	struct syscall_inode Inode = ReadInode(new_inode);
	int curr_dirent_block_num = Inode.direct[1];
	//printf("current dirent block num %d\n", curr_dirent_block_num);
	LogWrite("Read current directory's data block number\n");

	//Read directory entries datablock
	union syscall_block block;
	disk_read(curr_dirent_block_num, &block);

	//Update the currenty directory listing block
	//Note that the first array entry is number of dirents
	LogWrite("Updating current directory listing\n");

	//Create . directory	
	num_dirents += 1;
	strcpy(block.dir_entries[num_dirents].entry_name, ".");
	block.dir_entries[num_dirents].inode_num = new_inode;	

	//Create .. directory
	num_dirents++;
	strcpy(block.dir_entries[num_dirents].entry_name, "..");
	block.dir_entries[num_dirents].inode_num = parent_inode;

	//Set number of dirents to 2
	block.dir_entries[0].inode_num = num_dirents;
	disk_write(curr_dirent_block_num, &block);

	return 1;
}


int syscall_initial_filetable(){
	int i;
	//initialise all inode_numbers to -1.
	for(i=0;i<20;i++)
	{
		file_table_entries[i].inode_num=-1;
	}
	return 1;
}

/*
syscall_assign_filetable()
	-check the first available filetable entry	
	-assign it and declare it's inode number
*/
int syscall_assign_filetable(int inodenum){
	int i;
	//check the first available filetable entry 
	for(i=0;i<MAX_FD;i++)
	{
		if((file_table_entries[i].inode_num)==-1)
			//assign it and declare it's inode number
		{
			file_table_entries[i].inode_num=inodenum;
			//return the index for file open table
			LogWrite("Found file table entry\n");
			return i;
		}
	}
	LogWrite("No free file table entry\n");
	return -1;
}


void write_i_list_to_disk(){
	union syscall_block block;
	int k=0; //for every actual inode
	for(int i=1;i<=NUMBER_OF_INODE_BLOCKS;i++){ //for every inode block
		for(int j=0;j<INODES_PER_BLOCK;j++){//for every inode in the block
			block.inode[j]=i_list[k];
			k++;
		}
		disk_write(i,block.data);
		
	}
}
/*
Read specified inode from disk
*/
struct syscall_inode ReadInode(int inumber){
	//calculate block number for this inode
	int blocknum= calculate_block_for_inode(inumber);

	//read the specified block
	union syscall_block block;
	disk_read(blocknum,block.data);

	//calculate offset for this inode and block
	int offset= calculate_offset_in_block(inumber,blocknum);


	LogWrite("Read Inode\n");
	return block.inode[offset];
}


/*
syscall_delete 
	- Delete the inode indicated by the inumber
	- Release all data and indirect blocks assigned to this inode 
	- Return them to the free block map
	- On success, return one. On failure, return 0. 
*/
int syscall_delete_Inode( int inumber )
{
	if(inumber<0 || inumber> NUMBER_OF_INODES){
		LogWrite("Unable to delete specified inode\n");
		return 0;
	}
	//read the specified inode
	struct syscall_inode Inode = ReadInode(inumber);
	inode_atttributes_given_inode(Inode);

	//Make changes to the inode

	//for every data block
	for(int i=0;i<POINTERS_PER_INODE;i++){
		if(Inode.direct[i]!=-1){ //if a valid data block exists
			//free the data block in the bitmap
			//printf("\ndata block %d before freeing : %d",Inode.direct[i],free_block_bitmap[Inode.direct[i]]);
			free_block_bitmap[Inode.direct[i]]=0;
			//printf("\ndata block %d after freeing : %d",Inode.direct[i],free_block_bitmap[Inode.direct[i]]);
			//free the data block in the inode
			Inode.direct[i]=-1;
		}
	}
	Inode.isvalid=0; //make the inode invalid
	Inode.size=(int)sizeof(Inode);

	union syscall_block block;
	//Block and offset for this inode
	int blocknum=Inode.blocknum;
	int offset=Inode.offset_in_block;

	inode_atttributes_given_inode(Inode);

	//Read the specified block
	disk_read(blocknum,block.data);
	//set the inode in the block to the new modified one
	block.inode[offset]=Inode;
	//Write the changes made back to disk
	disk_write(blocknum,block.data);

	LogWrite("Deleted specified inode\n");
	return 1;
}

/*
syscall_getsize
 	-Return the logical size of the given inode, in bytes
 	- On failure, return -1. 
*/
int syscall_getsize( int inumber )
{
	if(inumber<=1 || inumber>NUMBER_OF_INODES){
		LogWrite("Getsize attempted on invalid inode");
		return -1;
	}
	struct syscall_inode Inode;
	Inode=ReadInode(inumber);
	return Inode.size;
}

/*
syscall_find_next_free_file_descriptor
	- go through free_file_desc
	- find next free one
	- set it to 1
*/
int syscall_find_next_free_file_descriptor(){
	for(int i=0;i<MAX_FD;i++){
		if(free_file_desc[i]==0){
			LogWrite("Found free file descriptor\n");
			free_file_desc[i]=-1;
			Open_file_table.count_used_file_descriptors++;
			return i;
		}
	}
	LogWrite("No free file descriptor found\n");
	return -1;
}

/*
	Adds a file entry to the parent inode (called when new file/dir is created)
*/
int syscall_add_entry_dir(int parent_inode, char *file_entry, int entry_inode){

	//Keeps track of number of directory entries
	int num_dirents;

	//Read the block containing the inode information
	struct syscall_inode Inode = ReadInode(parent_inode);
	int curr_dirent_block_num = Inode.direct[1];
	LogWrite("Read current directory's data block number\n");

	//Read directory entries datablock
	union syscall_block block;
	disk_read(curr_dirent_block_num, &block);

	//Update the currenty directory listing block
	//Note that the first array entry is number of dirents
	LogWrite("Updating current directory listing\n");
	num_dirents = block.dir_entries[0].inode_num;

	//Add directory	
	num_dirents++;
	strcpy(block.dir_entries[num_dirents].entry_name, file_entry);
	block.dir_entries[num_dirents].inode_num = entry_inode;	


	//Set number of dirents to 2
	block.dir_entries[0].inode_num = num_dirents;
	disk_write(curr_dirent_block_num, &block);

	return 1;
}

/*
syscall_find_fd_for_inodenum
	-FOR OPEN FILES
 	-return fd for inodenum
*/
int syscall_find_fd_for_inodenum(int inodenumber){
	// go through file_table_entries
	// find index for inode -> fd_pointer
	if(inodenumber<0 || inodenumber>NUMBER_OF_INODES){
		LogWrite("Invalid inode number\n");
		return -1;
	}
	int i=0;
	int fd_pointer=-1;
	while(i<MAX_FD){
		if(file_table_entries[i].inode_num==inodenumber){
			fd_pointer=i;
			i=MAX_FD;
		}
		i++;
	}
	if(fd_pointer==-1){
		LogWrite("Could not find inode number in file_table entry\n");
		return -1;
	}
	//go through fd_entry 
	i=0;
	int fd=-1;
	while(i<MAX_FD){
		if(Open_file_table.fd_entry[i].fd_pointer==fd_pointer){
			fd=i;
			i=MAX_FD;
		}
		i++;
	}

	return fd;
}


/*
syscall_find_inodenum_for_fd
	- Given fd find inode_num
*/
int syscall_find_inodenum_for_fd(int fd){
	if(fd<0 || fd>MAX_FD){
		LogWrite("Invalid fd\n");
		return -1;
	}

	//get fd_pointer in Open file table
	int fd_pointer=Open_file_table.fd_entry[fd].fd_pointer;

	//get inode in file_table_entries 
	int inodenumber=file_table_entries[fd_pointer].inode_num;

	return inodenumber;

}

/*
syscall_size_of_file_for_inodenum
	- return size of file in bytes
	- argument : inodenumber
	- ONLY DACTUAL DATA SIZE  (not stat)
*/
int syscall_size_of_file_for_inodenum(int inodenum){

	if(inodenum<0 || inodenum>NUMBER_OF_INODES){
		LogWrite("Invalid inode_num");
		return -1;
	}

	//read inode
	struct syscall_inode Inode = ReadInode(inodenum);

	return Inode.size;
}

/*
syscall_blocknum_for_offset
	- offset specifies where which byte we should start reading from
	- 4 datablocks per file, but only datablock 1,2,3,... have actual info 
		(0 is for stat)
*/
int syscall_blocknum_for_offset(offset){

	/* POINTERS_PER_INODE -1 =3
	*/
	if(offset<0 || offset>(DISK_BLOCK_SIZE*(POINTERS_PER_INODE-1))){
		LogWrite("Invalid offset\n");
		return -1;
	}
	int blocknum=1;
	while(offset/DISK_BLOCK_SIZE >= blocknum){
		blocknum++;
	}
	return blocknum;
}

int syscall_min(int a,int b){
	if(a<=b)return a;
	return b;
}

/*
syscall_find_stat_for_inodenum
	- Return stat structure for a inode
*/
struct stat syscall_find_stat_for_inodenum(int inodenum){

	struct syscall_inode Inode=ReadInode(inodenum);
	int stat_block=Inode.direct[0];
	union syscall_block block;
	disk_read(stat_block,&block);
	return block.stat_info;

}

//Write changes made in stat back to disk
int syscall_write_stat_to_disk(struct stat s,int inode_num){

	struct syscall_inode Inode=ReadInode(inode_num);
	union syscall_block stat_Block;
	stat_Block.stat_info=s;
	disk_write(Inode.direct[0],&stat_Block);
	return 1;
}


void syscall_display_current_file_info(){
	printf("\nCURRENT FILE INFO :");
	int inodes_in_use=0;
	int data_blocks_in_use=0;
	for(int i=0;i<NUMBER_OF_INODES;i++){
		if(i_list[i].isvalid){
			inodes_in_use++;
		}
	}
	for(int i=DATABLOCK_START;i<NUMBER_OF_BLOCKS;i++){
		if(free_block_bitmap[i]!=0){
			data_blocks_in_use++;
		}
	}
	printf("\n\tInodes in use : %d\n\tData Blocks in use : %d\n",inodes_in_use,data_blocks_in_use);
}

/* 
syscall_read 
	- help read bytes from a block
	- store read data into buf
*/

int  syscall_read( char *data, int bytes, int offset, char * buf)
{
	
//	printf("Actual data %s\n",data);
//	printf("bytes to be read %d from offset %d\n",bytes,offset);
	if(bytes<0)return 0;
	//buf=malloc(sizeof(char)*(bytes+1));
	strncpy(buf,data+offset,bytes);
	buf[bytes]='\0';
//	printf("syscall_read read into buf %s\n",buf);
	LogWrite("Syscall_read successfull\n");
	return 1;
}

int syscall_write( char *data, int bytes, int offset, char * buf )
{
	//printf("bytes to be read %d from offset %d\n",bytes,offset);
	if(bytes<0)return 0;
	//buf=malloc(sizeof(char)*(bytes+1));
	int new_offset=DISK_BLOCK_SIZE-offset;
	strncpy(data+new_offset,buf,bytes);
	data[bytes]='\0';
//	printf("syscall_read read into buf %s\n",buf);
	LogWrite("Syscall_read successfull\n");
	return 1;
}

