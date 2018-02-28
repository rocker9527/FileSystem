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

int file_open(char * pathname){
	
	printf("in file_open\n");
	//Find node from pathname
		//Check if file does not exist
		int path_inode = namei(pathname);
		printf("Inode of path name %s is  %d\n", pathname, path_inode);

		LogWrite("Path name resolved to inode number\n");
	// allocate file table entry for node, initialize count, offset;
	
	// allocate file descriptor entry, set pointer to file table entry;
	
	// if (type of open specifies truncate file)
		//free all file blocks (algorithm free);

	//return (file descriptor);
}

