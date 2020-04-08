/******************************************************************
CS 5348.001
Project 2 Part 1
Author: Gary Chen, Nancy Dominguez, Ju-Chen Lin

Program Description: This program will modify the Unix V6 file
system to eliminate the 16 MB limition. Functions include:
-initfs
-mkdir
-cpin
-cpout
-rm
-q
******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BLOCK_SIZE 1024 //satisfy project requirement
#define I_SIZE 32 //size of inode

//Flags
#define inode_alloc 0100000			//i-node is allocated 
#define directory 040000			//directory
#define pfile 000000               //To define file as a plain file
#define lfile 010000               //To define file as a large file

#define array_max_chain 256			//int = 4 bytes, 256 * 4 = 1024 bytes.

#define FREE_ARRAY_SIZE 148			//increase free array size so that most of the superblock will be used

/*************** super block structure**********************/
//double all fields of the superblock so that most of the superblock will be used
typedef struct {
	unsigned int isize;
	unsigned int fsize;
	unsigned int nfree;
	unsigned int free[FREE_ARRAY_SIZE];
	unsigned int ninode;
	unsigned int inode[100];
	short flock;
	short ilock;
	short fmod;
	unsigned int time[2];
} superblock;

/************** directory contents***************************/
typedef struct
{
	unsigned short inode;
	char filename[14];
}dir;

/****************inode structure ************************/
typedef struct {
	unsigned short flags;
	char nlinks;
	char uid;
	char gid;
	char size0;
	unsigned short size1;
	unsigned short addr[8];
	unsigned short actime[2];
	unsigned short modtime[2];
} inode;

inode initial_inode;
superblock super;

int fd;		//file descriptor
int rootfd;
const char *rootname;
unsigned int array_chain[array_max_chain];

int option = 0;

dir newdir;

/*************** functions used *****************/
int initfs(char* path, unsigned int parse_blocks, unsigned int total_inodes);
void rootDir();
void blockRead(unsigned int *target, unsigned int block_entry_num);
void blockToArray(unsigned int *target, unsigned int block_entry_num);
void writeToInode(inode current_inode, unsigned int new_inode);
void blockChain(unsigned int parse_blocks);
unsigned int getFreeBlock();
unsigned int addFreeBlock(int block_num);
int makeDir(const char *pathname, unsigned short block_num);
int traversePath(const char *pathname, unsigned short block_num);
void update_rootdir(const char *pathname, unsigned short in_number);
void cpin(const char *ext_file, const char *sys_file, unsigned short inode_num, char *pathname);
void cpin_plainfile(const char *pathname1, const char *pathname2, int req_blocks, unsigned short inode_num);
void cpin_largefile(const char *pathname1, const char *pathname2, int req_blocks, unsigned short inode_num);
void cpout(const char *ext_file, const char *sys_file, unsigned short inode_num, char *pathname);
int searchCreateFile(const char *pathname, unsigned short block_num, int cp_option);
void cpout_plainfile(const char *ext_file, const char *sys_file, inode file_inode);
void cpout_largefile(const char *ext_file, const char *sys_file, inode file_inode);
void remove_file(const char *sys_file, unsigned short inode_num, unsigned short block_num);
void remove_plainfile(const char *sys_file, unsigned short inode_num, unsigned short block_num, inode rmFile_inode);
void remove_largefile(const char *sys_file, unsigned short inode_num, unsigned short block_num, inode rmFile_inode);
unsigned short allocate_inode();
unsigned short deallocate_inode(unsigned short inode_num);

/**************** Main function ********************/
int main(int argc, char *argv[])
{
	char c;

	printf("\n Clearing screen \n");
	system("clear");
	int i = 0;
	char *tmp = (char *)malloc(sizeof(char) * 200);

	char *cmd1 = (char *)malloc(sizeof(char) * 200);
	signal(SIGINT, SIG_IGN);

	printf("\n\nModified_Unix_File_system_V6 ");
	fflush(stdout);

	int fs_count = 0;

	char *cmd_arg, cmd[256], copy[256];
	unsigned int n = 0, j, k;
	char buffer1[BLOCK_SIZE];
	unsigned int num_of_bytes;
	char *dir_name;
	char *ext_file_name;
	char *filesys_name;

	unsigned int block_num = 0, inode_num = 0;
	char *fs_path;
	char *arg1, *arg2;

	//char *pathname;

	printf("\nSystem >>");

	while (1)
	{
		char *temp = (char*)malloc(256 * sizeof(char));
		char *pathname = (char*)malloc(256 * sizeof(char));
		scanf(" %[^\n]s", cmd);
		strncpy(&temp[0], cmd, sizeof(cmd));
		strncpy(&pathname[0], cmd, sizeof(cmd));
		cmd_arg = strtok(temp, " ");

		//initfs command
		if (strcmp(cmd_arg, "initfs") == 0)
		{

			fs_path = strtok(NULL, " ");
			arg1 = strtok(NULL, " ");
			arg2 = strtok(NULL, " ");

			if (access(fs_path, F_OK) == 0)
			{
				printf("The file system already exists. \n");
				printf("The same file system will be used. \n");
				fs_count = 1;
			}
			else
			{
				if (!arg1 || !arg2)
					printf("Not enough arguments. \n");
				else
				{
					block_num = atoi(arg1);
					inode_num = atoi(arg2);

					if (initfs(fs_path, block_num, inode_num) == 1)
					{
						printf("File system now initialized. \n");
						fs_count = 1;
					}
					else
					{
						printf("Error: Unable to initialize file system. \n");
					}
				}
			}
			cmd_arg = NULL;
			for (i = 0; i < sizeof(cmd); i++)
				cmd[i] = '.';
			free(temp);
			printf("\nSystem >>");
		}

		//mkdir command
		else if (strcmp(cmd_arg, "mkdir") == 0)
		{

			unsigned short block_num = initial_inode.addr[0];

			if (fs_count == 0)
				printf("No file system has been initialized. Retry after initializing a file system\n");
			else
			{
				int path_length = 0;
				int path_iter = 0;
				int current_path = 1;

				for (i = 0; i < sizeof(cmd); i++)
				{
					if (cmd[i] == '/')
						path_length++;
				}

				dir_name = strtok(NULL, "/");

				if (current_path == path_length)	//check if only one pathname
				{
					int m = makeDir(dir_name, block_num);
					if (m == -1)
					{
						printf("Directory already exists\n");
					}
					else if (m == 1)
					{
						printf("Directory created successfully\n");
						
					}
				}
				else//check through all pathnames
				{
					while (dir_name != NULL)
					{
						if (current_path < path_length)
						{
							block_num = traversePath(dir_name, block_num);
							if (block_num == -1)
							{
								printf("Path does not exists.\n");
								break;
							}
							else
								current_path++;
						}
						else if (current_path == path_length)
						{
							int m = makeDir(dir_name, block_num);
							if (m == -1)
							{
								printf("Directory already exists\n");
								break;
							}
							else if (m == 1)
							{
								printf("Directory created successfully.\n");
								break;
							}
						}
						dir_name = strtok(NULL, "/");
					}
				}
				current_path = 1;
			}
			cmd_arg = NULL;
			for (i = 0; i < sizeof(cmd); i++)
				cmd[i] = '.';
			free(temp);
			printf("\nSystem >>");
		}

		else if (strcmp(cmd_arg, "cpin") == 0)
		{
		unsigned short block_num = initial_inode.addr[0];
		//bool stop = false;
			if (fs_count == 0)
				printf("Can't locate init_FS. Retry after initializing file system\n");
			else
			{
				int path_length = 0;
				int path_iter = 0;
				int current_path = 1;

				for (i = 0; i < sizeof(cmd); i++)
				{
					if (cmd[i] == '/')
						path_length++;
				}

				ext_file_name = strtok(NULL, " ");
				filesys_name = strtok(NULL, "/");

				//pathname[current_path - 1] = filesys_name;
				
				if (current_path == path_length)	//check if only one pathname
				{
					int file_inum = searchCreateFile(filesys_name, block_num, 0);
				}
				else
				{
					int file_inum;
					while (filesys_name != NULL)
					{
						if (current_path < path_length)
						{
							block_num = traversePath(filesys_name, block_num);
							if (block_num == -1)
							{
								printf("Path does not exists.\n");
								break;
							}
							else
							{
								current_path++;
								//pathname[current_path - 1] = filesys_name;
							}
						}
						else if (current_path == path_length)
						{
							file_inum = searchCreateFile(filesys_name, block_num, 0);
						}
						filesys_name = strtok(NULL, "/");
					}

					if (!ext_file_name)
					{
						printf("Error: Cannot find external file.\n");
						//stop = true;
					}
					else
					{
						cpin(ext_file_name, filesys_name, file_inum, pathname);
					}
				}
			}
			cmd_arg = NULL;
			printf("\nSystem >>");
		}

		else if (strcmp(cmd_arg, "cpout") == 0)
		{
		unsigned short block_num = initial_inode.addr[0];
		//bool stop = false;
		if (fs_count == 0)
			printf("Can't locate init_FS. Retry after initializing file system\n");
		else
		{
			int path_length = 0;
			int path_iter = 0;
			int current_path = 1;

			for (i = 0; i < sizeof(cmd); i++)
			{
				if (cmd[i] == '/')
					path_length++;
			}

			
			filesys_name = strtok(NULL, "/");

			if (current_path == path_length)	//check if only one pathname
			{
				int file_inum = searchCreateFile(filesys_name, block_num, 1);
			}
			else
			{
				int file_inum;
				while (filesys_name != NULL)
				{
					if (current_path < path_length)
					{
						block_num = traversePath(filesys_name, block_num);;
						if (block_num == -1)
						{
							printf("Path does not exists.\n");
							break;
						}
						else
							current_path++;
					}
					else if (current_path == path_length)
					{
						file_inum = searchCreateFile(filesys_name, block_num, 1);
						break;
					}
					if (current_path < path_length)
						filesys_name = strtok(NULL, "/");
					else if (current_path = path_length)
						filesys_name = strtok(NULL, " ");
				}

				ext_file_name = strtok(NULL, " ");

				if (file_inum == -1)
				{
					printf("Error: Internal file does not exist.\n");
				}
				else
				{
					cpout(ext_file_name, filesys_name, file_inum, pathname);
				}
			}
		}
		cmd_arg = NULL;
		printf("\nSystem >>");
		}

		else if (strcmp(cmd_arg, "rm") == 0)
		{
		unsigned short block_num = initial_inode.addr[0];
		//bool stop = false;
		if (fs_count == 0)
			printf("Can't locate init_FS. Retry after initializing file system\n");
		else
		{
			int path_length = 0;
			int path_iter = 0;
			int current_path = 1;

			for (i = 0; i < sizeof(cmd); i++)
			{
				if (cmd[i] == '/')
					path_length++;
			}

			
			filesys_name = strtok(NULL, "/");

			if (current_path == path_length)	//check if only one pathname
			{
				int file_inum = searchCreateFile(filesys_name, block_num, 1);
			}
			else
			{
				int file_inum;
				while (filesys_name != NULL)
				{
					if (current_path < path_length)
					{
						block_num = traversePath(filesys_name, block_num);;
						if (block_num == -1)
						{
							printf("Path does not exists.\n");
							break;
						}
						else
							current_path++;
					}
					else if (current_path == path_length)
					{
						file_inum = searchCreateFile(filesys_name, block_num, 1);
						break;
					}
					filesys_name = strtok(NULL, "/");
				}

				if (file_inum == -1)
				{
					printf("Internal file does not exists.\n");
				}
				else
					remove_file(filesys_name, file_inum, block_num);
			}
		}
		cmd_arg = NULL;
		printf("\nSystem >>");
		}
		//show all files
		else if (strcmp(cmd_arg, "ls") == 0)
		{
			//      show_all_files();
			printf("listing");
			fd = open(arg1, O_RDWR, 0600);
			printf("fd >>> %d", fd);
			lseek(fd, 2 * BLOCK_SIZE * sizeof(inode), SEEK_SET);

			read(fd, &initial_inode, sizeof(initial_inode));
			dir temp_entry;
			int index = 0;

			while (index < initial_inode.size0 / sizeof(dir))
			{
				if (index % (BLOCK_SIZE / sizeof(dir)) == 0)
				{
					lseek(fd, offset_set(initial_inode.addr[index / (BLOCK_SIZE / sizeof(dir))]), SEEK_SET);
				}
				read(fd, &temp_entry, sizeof(temp_entry));
				printf("%-d %s \n", temp_entry.inode, temp_entry.filename);
				index++;
			}
			printf("\nSystem >>");
		}

		// quit command
		else if (strcmp(cmd_arg, "q") == 0)
		{
			printf("\nExiting file system ..............");
			printf("\nThank you!\n");
			lseek(fd, BLOCK_SIZE, 0);

			//saving contents of superblock structure into superblock in file system
			if ((num_of_bytes = write(fd, &super, BLOCK_SIZE)) < BLOCK_SIZE)
			{
				printf("Error: Unable to write to super block\n");
			}
			lseek(fd, BLOCK_SIZE, 0);
			return 0;
		}
		else
		{
			printf("Invalid command, please type:\n initfs <pathname> <number of blocks> <number of i-nodes>\n ");
			printf("mkdir <directory name>\n ");
			printf("cpin <external file name> <system pathname>\n ");
			printf("cpout <system pathname> <external file name>\n ");
			printf("rm <system pathname>\n ");
			printf("q\n ");
			printf("\nSystem >>");

		}
	}
}

//function to initialize file system
int initfs(char* path, unsigned int parse_blocks, unsigned int total_inodes)
{
	printf("\nBeginning file system intialization... \n");
	char buffer[BLOCK_SIZE];
	int num_of_bytes;
	if (((total_inodes * 32) % 1024) == 0) // total inodes * size of inode(bytes) % block size = remainder inodes
		super.isize = ((total_inodes * 32) / 1024); //set number of blocks devoted to inodes to this if there are no leftover inodes
	else
		super.isize = ((total_inodes * 32) / 1024) + 1; //set number of blocks devoted to inodes to this if there are leftover inodes

	super.fsize = parse_blocks;

	unsigned int i = 0;

	if ((fd = open(path, O_RDWR | O_CREAT, 0600)) == -1)
	{
		printf("\n Error: Unable to open file [%s]\n", strerror(errno));
		return -1;
	}

	for (i = 0; i < FREE_ARRAY_SIZE; i++) //initialize free array
		super.free[i] = 0;

	super.nfree = 0; //set number of free blocks = 0


	super.ninode = 100; //set number of free i-numbers in inode arr
	for (i = 0; i < 100; i++)
		super.inode[i] = i;

	//flags maintained in core copy of file system
	super.flock = 'f';
	super.ilock = 'i';
	super.fmod = 'f';
	//number of seconds passed since Jan 1 1970
	super.time[0] = 0000;
	super.time[1] = 1970;
	lseek(fd, BLOCK_SIZE, SEEK_SET); //go to block 1 (super block)

	//lseek(fd, 0, SEEK_SET);
	num_of_bytes = write(fd, &super, BLOCK_SIZE); //write super block structure to superblock in file system

	if (num_of_bytes < BLOCK_SIZE)
	{
		printf("\n Error: Unable to write to super block\n");
		return -1;
	}

	//set all inodes to 0
	for (i = 0; i < BLOCK_SIZE; i++)
		buffer[i] = 0;
	lseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	for (i = 0; i < super.isize; i++)
		write(fd, buffer, BLOCK_SIZE);

	blockChain(parse_blocks);

	//fill free array with the data block numbers (position)
	for (i = 0; i < FREE_ARRAY_SIZE; i++)
	{

		super.free[super.nfree] = i + 2 + super.isize;
		++super.nfree;

	}

	rootDir();

	return 1;
}

//function to read target integer array from the required block
void blockRead(unsigned int *target, unsigned int block_entry_num)
{
	int flag = 0;
	if (block_entry_num > super.isize + super.fsize)
		flag = 1;

	else {
		lseek(fd, block_entry_num*BLOCK_SIZE, SEEK_SET);
		read(fd, target, BLOCK_SIZE);
	}
}

//function to write target integer array to the required block
void blockToArray(unsigned int *target, unsigned int block_entry_num)
{
	int flag1, flag2;
	int num_of_bytes;

	if (block_entry_num > super.isize + super.fsize)
		flag1 = 1;
	else {

		lseek(fd, block_entry_num*BLOCK_SIZE, SEEK_SET);
		num_of_bytes = write(fd, target, BLOCK_SIZE);

		if ((num_of_bytes) < BLOCK_SIZE)
			flag2 = 1;
	}
	if (flag2 == 1)
	{
		printf("\nError in block number %d\n", block_entry_num);
	}
}

//function to create root directory and its corresponding inode.
void rootDir()
{
	rootname = "root";
	rootfd = creat(rootname, 0600);
	rootfd = open(rootname, O_RDWR | O_APPEND);
	unsigned int i = 0;
	unsigned int num_of_bytes;
	unsigned int datablock = getFreeBlock();
	for (i = 0; i < 14; i++)
		newdir.filename[i] = 0;

	newdir.filename[0] = '.';			//root directory's file name is .
	newdir.filename[1] = '\0';
	newdir.inode = 1;					// root directory's inode number is 1.

	//setting inode structure for root
	initial_inode.flags = inode_alloc | directory | 000007;   		// flag for root directory
	initial_inode.nlinks = 2;
	initial_inode.uid = '0';
	initial_inode.gid = '0';
	initial_inode.size0 = '0';
	initial_inode.size1 = I_SIZE;
	initial_inode.addr[0] = datablock;
	for (i = 1; i < 8; i++)
		initial_inode.addr[i] = 0;

	initial_inode.actime[0] = 0;
	initial_inode.actime[1] = 0;
	initial_inode.modtime[0] = 0;
	initial_inode.modtime[1] = 0;

	//write data in initial inode structure to the initial inode in file system
	writeToInode(initial_inode, 1);
	//lseek(fd, 1024, SEEK_SET);
	//write(fd, &initial_inode, 64);
	lseek(fd, datablock*BLOCK_SIZE, SEEK_SET);

	//write root directory first entry . to file system
	num_of_bytes = write(fd, &newdir, 16);
	if ((num_of_bytes) < 16)
		printf("\n Error: Unable to write root directory \n ");

	newdir.filename[0] = '.';
	newdir.filename[1] = '.';
	newdir.filename[2] = '\0';

	//write root directory second entry .. to file system
	num_of_bytes = write(fd, &newdir, 16);
	if ((num_of_bytes) < 16)
		printf("\n Error: Unable to write root directory\n ");
	close(rootfd);
}

// data block chaining procedure for free array
void blockChain(unsigned int parse_blocks)
{
	unsigned int emptybuffer[256]; // buffer to fill entire blocks with zeros (int = 4 bytes, 256 * 4 = 1024 bytes)
	unsigned int link_count;
	unsigned int partitions_blocks = parse_blocks / FREE_ARRAY_SIZE; //data blocks partitioned evenly
	unsigned int blocks_not_in_partitions = parse_blocks % FREE_ARRAY_SIZE; //remainder blocks
	unsigned int index = 0;
	int i = 0;
	//setting arrays to 0 to remove bad data
	for (index = 0; index <= 255; index++)
	{
		emptybuffer[index] = 0;
		array_chain[index] = 0;
	}

	//chaining of blocks 148 blocks at a time
	for (link_count = 0; link_count < partitions_blocks; link_count++)
	{
		array_chain[0] = FREE_ARRAY_SIZE; //number of free blocks listed in the next 148 words

		for (i = 0; i < FREE_ARRAY_SIZE; i++)
		{
			//if data blcoks are evenly partitioned, and it is the last partition, and we're looking at index 0 of that partition
			if ((link_count == (partitions_blocks - 1)) && (blocks_not_in_partitions == 0) && (i == 0))
			{
				array_chain[i + 1] = 0; //point to nothing
				continue;
			}

			//adding free data block numbers to array_chain (we will only be keeping the first word of each partition later)
			array_chain[i + 1] = i + (FREE_ARRAY_SIZE * (link_count + 1)) + (super.isize + 2);

		}
		//write array_chain to file system
		blockToArray(array_chain, 2 + super.isize + FREE_ARRAY_SIZE * link_count);

		//only keep the first word of each partition and set all other words to 0
		for (i = 1; i <= FREE_ARRAY_SIZE; i++)
			blockToArray(emptybuffer, 2 + super.isize + i + FREE_ARRAY_SIZE * link_count);
	}

	//chaining for remaining blocks
	array_chain[0] = blocks_not_in_partitions;	//number of free blocks listed in the last remainder of words
	array_chain[1] = 0;							//this is the last "partition" block so it should not point to anything
	//adding free data block numbers to array_chain (we will only be keeping the first word of this partition later)
	for (i = 1; i <= blocks_not_in_partitions; i++)
		array_chain[i + 1] = 2 + super.isize + i + (FREE_ARRAY_SIZE * link_count);

	//write array_chain to file system
	blockToArray(array_chain, 2 + super.isize + (FREE_ARRAY_SIZE * link_count));

	//only keep the first word of this partition and set all other words to 0
	for (i = 1; i <= blocks_not_in_partitions; i++)
		blockToArray(emptybuffer, 2 + super.isize + 1 + i + (FREE_ARRAY_SIZE * link_count));
}


//function to write to an inode given the inode number
void writeToInode(inode current_inode, unsigned int new_inode)
{
	int num_of_bytes;
	lseek(fd, 2 * BLOCK_SIZE + (new_inode - 1) * I_SIZE, SEEK_SET);	//go to byte of inode that we're interested in
	num_of_bytes = write(fd, &current_inode, I_SIZE);			//write data in inode structure to inode block in file system

	if ((num_of_bytes) < I_SIZE)
		printf("\n Error in inode number %d\n", new_inode);
}

//function to get a free data block and decrements nfree in each pass
unsigned int getFreeBlock()
{
	unsigned int block;

	super.nfree--;						//decrement nfree
	block = super.free[super.nfree];	//new block is free[nfree]
	super.free[super.nfree] = 0;

	//if nfree became 0:
	if (super.nfree == 0)
	{
		int n = 0;
		blockRead(array_chain, block);			//read in the block named by the new block number
		super.nfree = array_chain[0];			//replace nfree by its first word
		for (n = 0; n < FREE_ARRAY_SIZE; n++)	//copy the block numbers in the next 148 words into free arrray
			super.free[n] = array_chain[n + 1];
	}
	return block;
}
unsigned int addFreeBlock(int block_num)
{
	if (super.nfree == FREE_ARRAY_SIZE)
	{
		lseek(fd, block_num*BLOCK_SIZE, SEEK_SET);
		write(fd, &super.nfree, sizeof(int));
		write(fd, &super.free, sizeof(super.free));

		super.nfree = 0;
	}

	super.free[super.nfree] = block_num;
	super.nfree++;

}

/************ Function to create a Directory*******************/
int makeDir(const char *pathname, unsigned short block_num)
{
	int i = 0, j = 0;
	int offset = 0;
	inode inode1;
	dir dir1;
	dir parent;
	char buff[50];

	i = 0;

	lseek(fd, block_num * BLOCK_SIZE, SEEK_SET);  //go to root directory
	read(fd, &dir1, sizeof(dir)); //offset after 1st entry
	read(fd, &dir1, sizeof(dir)); //offset after 2nd entry
	offset = lseek(fd, 0, SEEK_CUR); //same offset
	read(fd, &dir1, sizeof(dir1)); //offset after 3rd entry

	parent = dir1;

	int match_count = 0;
	while(dir1.inode != NULL || dir1.inode != 0)
	{
		for (i = 0; i < strlen(pathname); i++)
		{
			if (dir1.filename[i] == pathname[i])
				match_count++;
		}
	
			if (match_count == strlen(pathname))
			{
				return -1;	//should halt and give error message: directory already exists
			}

		offset = lseek(fd, 0, SEEK_CUR);
		read(fd, &dir1, sizeof(dir1)); //reads the next entries starting with 4th
	}

	//Store directory entry into the parent block
	dir dir2;

	int inum = allocate_inode();

	dir2.inode = inum;
	for (j = 0; j < 14; j++)
	{
		dir2.filename[j] = pathname[j];
	}

	lseek(fd, offset, SEEK_SET); //offset is where we left off at the parent block
	write(fd, &dir2, 16);  //offset is at end of created entry

	int block = getFreeBlock();

	//setting inode structure for for new directory
	inode new_Inode;

	new_Inode.flags = inode_alloc | directory | 000007;   		// flag for root directory
	new_Inode.nlinks = 2;
	new_Inode.uid = '0';
	new_Inode.gid = '0';
	new_Inode.size0 = '0';
	new_Inode.size1 = I_SIZE;
	new_Inode.addr[0] = block;
	for (i = 1; i < 8; i++)
		new_Inode.addr[i] = 0;

	new_Inode.actime[0] = 0;
	new_Inode.actime[1] = 0;
	new_Inode.modtime[0] = 0;
	new_Inode.modtime[1] = 0;
	
	writeToInode(new_Inode, inum);

	//Write the directory entry to block
	dir newdir;
	newdir.inode = dir2.inode;
	newdir.filename[0] = '.';
	newdir.filename[1] = '\0';

	lseek(fd, block*BLOCK_SIZE, SEEK_SET); //offset is at newDir directory block
	write(fd, &newdir, sizeof(dir)); //offset is after 1st entry

	newdir.inode = parent.inode;
	newdir.filename[0] = '.';
	newdir.filename[1] = '.';
	newdir.filename[2] = '\0';

	write(fd, &newdir, sizeof(dir)); //offset is after 2nd entry

	return 1;
}

int traversePath(const char *pathname, unsigned short block_num)
{
	int i = 0, j = 0;
	int offset = 0;
	inode inode1;
	dir dir1;
	dir parent;
	char buff[50];

	i = 0;

	//Seek to first inode
	lseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	read(fd, &inode1, sizeof(inode1));

	lseek(fd, block_num* BLOCK_SIZE, SEEK_SET);  //go to root directory
	read(fd, &dir1, sizeof(dir)); //offset after 1st entry
	read(fd, &dir1, sizeof(dir)); //offset after 2nd entry
	offset = lseek(fd, 0, SEEK_CUR); //same offset
	read(fd, &dir1, sizeof(dir1)); //offset after 3rd entry

	int match_count = 0;	//to count the number of matching chars up to pathname length

	while (dir1.inode != NULL || dir1.inode != 0)
	{
		//Determining if pathname is found
		for (i = 0; i < strlen(pathname); i++)
		{
			if (dir1.filename[i] == pathname[i])
				match_count++;
		}

		if (match_count == strlen(pathname))
		{
			break;
		}
		else
			return -1;	//should halt and give error message: directory path does not exists


		offset = lseek(fd, 0, SEEK_CUR);
		read(fd, &dir1, sizeof(dir1)); //reads the next entries starting with 4th
	}

	//go to inode and retrieve target block number (second to last block)
	lseek(fd, 2 * BLOCK_SIZE + (dir1.inode - 1)*I_SIZE, SEEK_SET);
	read(fd, &inode1, sizeof(inode));

	//return target block number
	return inode1.addr[0];
}

/************ Function to update root directory*******************/
void update_rootdir(const char *pathname, unsigned short in_number)
{
	int i;
	dir ndir;
	int size;
	ndir.inode = in_number;

	strncpy(ndir.filename, pathname, 14);
	rootfd = open(rootname , O_RDWR | O_APPEND);

	size = write(rootfd, &ndir, 16);
	close(rootfd);
}

/************** Function to copy file contents from external file to mv6 filesystem************/
void cpin(const char *ext_file, const char *sys_file, unsigned short inode_num, char *pathname)

{
	//For displaying pathname
	sys_file = strtok(pathname, " ");
	sys_file = strtok(NULL, " ");
	sys_file = strtok(NULL, " ");

	struct stat stats;
	int blksize, blks_allocated, req_blocks;
	int filesize;
	stat(ext_file, &stats);
	filesize = stats.st_size;

	if (filesize % BLOCK_SIZE == 0)
		req_blocks = filesize / BLOCK_SIZE;			//if no remainder
	else
		req_blocks = filesize / BLOCK_SIZE + 1;		//if there is a remainder, add 1 more block

	//small file
	if (req_blocks <= 8)
	{
		printf("Copying small external file '%s' to system file '%s'\n", ext_file, sys_file);
		//printf("plain file , %d\n", req_blocks);
		cpin_plainfile(ext_file, sys_file, req_blocks, inode_num); // If the external is a small file
	}
	//large file
	else if (req_blocks >= 9 && req_blocks <= 67328)
	{
		printf("Copying large external file '%s' to system file '%s'\n", ext_file, sys_file);
		//printf("Large file , %d\n", req_blocks);
		cpin_largefile(ext_file, sys_file, req_blocks, inode_num); // If external file is a large file
	}
	//extra large file
	else
		printf("\nExternal File too large\n");

}

void cpin_plainfile(const char *ext_file, const char *sys_file, int req_blocks, unsigned short inode_num)
{
	int i;
	int fdex;
	char buffer[BLOCK_SIZE];

	//NULL buffer
	for (i = 0; i < BLOCK_SIZE; i++)
		buffer[i] = '\0';

	inode new_Inode;

	new_Inode.flags = inode_alloc | pfile | 000007;   		// flag for root directory
	new_Inode.nlinks = 2;
	new_Inode.uid = '0';
	new_Inode.gid = '0';
	new_Inode.size0 = '0';
	new_Inode.size1 = I_SIZE;
	for (i = 0; i < req_blocks; i++)
		new_Inode.addr[i] = getFreeBlock();

	for (i = req_blocks; i < sizeof(new_Inode.addr); i++)
		new_Inode.addr[i] = 0;

	new_Inode.actime[0] = 0;
	new_Inode.actime[1] = 0;
	new_Inode.modtime[0] = 0;
	new_Inode.modtime[1] = 0;

	fdex = open(ext_file, O_RDONLY);
	for (i = 0; i < req_blocks; i++)
	{
		//printf("~Number of req_blocks %d~", req_blocks);
		lseek(fd, new_Inode.addr[i] * BLOCK_SIZE, SEEK_SET);
		read(fdex, &buffer, BLOCK_SIZE);
		write(fd, &buffer, BLOCK_SIZE);
	}

	writeToInode(new_Inode, inode_num);

	close(fdex);
}

/************ Function to copy from a Large File into mv6 File system*******************/
void cpin_largefile(const char *ext_file, const char *sys_file, int req_blocks, unsigned short inode_num)
{
	int i, j, k;
	int ind_blocks;
	int sec_ind_blocks;
	int rem_blocks;
	int sec_rem_blocks;
	int extra;
	int new_ind_block, new_sec_ind_block, new_sec_rem_block;
	int offset, offset2, offset3;
	int fdex;
	char buffer[BLOCK_SIZE];
	int d;
	int inbuff[BLOCK_SIZE];
	for (i = 0; i < BLOCK_SIZE; i++)
		buffer[i] = '\0';

	int blk_count = 0;
	inode new_Inode;

	new_Inode.flags = inode_alloc | lfile | 000007;   		// flag for root directory
	new_Inode.nlinks = 2;
	new_Inode.uid = '0';
	new_Inode.gid = '0';
	new_Inode.size0 = '0';
	new_Inode.size1 = I_SIZE;
	for (i = 0; i < req_blocks; i++)
		new_Inode.addr[i] = getFreeBlock();

	for (i = req_blocks; i < sizeof(new_Inode.addr); i++)
		new_Inode.addr[i] = 0;

	new_Inode.actime[0] = 0;
	new_Inode.actime[1] = 0;
	new_Inode.modtime[0] = 0;
	new_Inode.modtime[1] = 0;

	//int val = 0, val1;
	fdex = open(ext_file, O_RDONLY);
	
	//find the number of indirect blocks and remainder blocks
	if (req_blocks <= 7 * 256)		//if only large file
	{
		if (req_blocks % 256 == 0)
		{
			ind_blocks = req_blocks / 256;
			rem_blocks = 0;
		}
		else
		{
			ind_blocks = req_blocks / 256 + 1;
			rem_blocks = req_blocks % 256;
		}
	}
	else      //if extra large file
	{
		ind_blocks = 8;
		rem_blocks = 0;
		extra = req_blocks - (7 * 256);
		if (extra % 256 == 0)
		{
			sec_ind_blocks = extra / 256;
			sec_rem_blocks = 0;
		}
		else
		{
			sec_ind_blocks = extra / 256 + 1;
			sec_rem_blocks = extra % 256;
		}
	}

	//printf("~%d %d %d %d~", req_blocks, extra, sec_ind_blocks, sec_rem_blocks);
	
	i = 0;
	while (i < ind_blocks)
	{
		if(i < 7)	//large file
		{
			new_Inode.addr[i] = getFreeBlock();
			blk_count++;
			offset = lseek(fd, new_Inode.addr[i]*BLOCK_SIZE, SEEK_SET);		//go to first indirect block

			//loop through direct blocks and check for remainders
			if (rem_blocks != 0 && i == ind_blocks-1)
			{
				for (j = 0; j < rem_blocks; j++)
				{
					lseek(fd, offset, SEEK_SET);

					new_ind_block = getFreeBlock();
					blk_count++;

					write(fd, &new_ind_block, sizeof(int));

					lseek(fd, offset, SEEK_SET);
					read(fd, &new_ind_block, sizeof(int));

					if (new_ind_block == 0)
					{
						new_ind_block = getFreeBlock();
						lseek(fd, offset, SEEK_SET);
						write(fd, &new_ind_block, sizeof(int));
					}

					read(fdex, &buffer, BLOCK_SIZE);

					lseek(fd, new_ind_block*BLOCK_SIZE, SEEK_SET);
					write(fd, &buffer, BLOCK_SIZE);

					offset = offset + sizeof(int);

					
				}
				for (j = rem_blocks; j < 256; j++)
				{
					int zero = 0;
					lseek(fd, offset, SEEK_SET);
					write(fd, &zero, sizeof(int));
					offset = offset + sizeof(int);
				}
			}
			else
			{
				for (j = 0; j < 256; j++)
				{
					lseek(fd, offset, SEEK_SET);

					new_ind_block = getFreeBlock();
					blk_count++;
					write(fd, &new_ind_block, sizeof(int));

					lseek(fd, offset, SEEK_SET);
					read(fd, &new_ind_block, sizeof(int));

					if (new_ind_block == 0)
					{
						new_ind_block = getFreeBlock();
						lseek(fd, offset, SEEK_SET);
						write(fd, &new_ind_block, sizeof(int));
					}

					read(fdex, &buffer, BLOCK_SIZE);
					lseek(fd, new_ind_block*BLOCK_SIZE, SEEK_SET);
					write(fd, &buffer, BLOCK_SIZE);

					offset = offset + sizeof(int);
				}
			}
			j = 0;
		}
		else if (i == 7)
		{
			new_Inode.addr[i] = getFreeBlock();
			blk_count++;
			offset = lseek(fd, new_Inode.addr[i]*BLOCK_SIZE, SEEK_SET);

			while(j < sec_ind_blocks)
			{
				new_sec_ind_block = getFreeBlock();
				write(fd, &new_sec_ind_block, sizeof(int));
				offset2 = lseek(fd, new_sec_ind_block * BLOCK_SIZE, SEEK_SET);

				if (sec_rem_blocks != 0 && j == sec_ind_blocks-1)		//remainder sec_rem_blocks
				{
					for (k = 0; k < sec_rem_blocks; k++)
					{
						lseek(fd, offset2, SEEK_SET);

						new_sec_rem_block = getFreeBlock();
						blk_count++;
						write(fd, &new_sec_rem_block, sizeof(int));


						lseek(fd, offset2, SEEK_SET);
						read(fd, &new_sec_rem_block, sizeof(int));

						if (new_sec_rem_block == 0)
						{
							new_sec_rem_block = getFreeBlock();
							lseek(fd, offset2, SEEK_SET);
							write(fd, &new_sec_rem_block, sizeof(int));
						}

						read(fdex, &buffer, BLOCK_SIZE);
						lseek(fd, new_sec_rem_block*BLOCK_SIZE, SEEK_SET);
						write(fd, &buffer, BLOCK_SIZE);

						offset2 = offset2 + sizeof(int);
					}
					for (k = sec_rem_blocks; k < 256; k++)
					{
						int zero = 0;
						lseek(fd, offset2, SEEK_SET);
						write(fd, &zero, sizeof(int));
						offset2 = offset2 + sizeof(int);
					}
					

					offset = lseek(fd, offset+sec_ind_blocks*sizeof(int), SEEK_SET);		//remainder sec_ind_blocks
					for (j = sec_ind_blocks; j < 256; j++)
					{
						int zero = 0;
						lseek(fd, offset, SEEK_SET);
						write(fd, &zero, sizeof(int));
						offset = offset + sizeof(int);
					}
				}
				else
				{
					for (k = 0; k < 256; k++)
					{
						lseek(fd, offset2, SEEK_SET);

						new_sec_rem_block = getFreeBlock();
						blk_count++;
						write(fd, &new_sec_rem_block, sizeof(int));

						lseek(fd, offset2, SEEK_SET);
						read(fd, &new_sec_rem_block, sizeof(int));

						if (new_sec_rem_block == 0)
						{
							new_sec_rem_block = getFreeBlock();
							lseek(fd, offset2, SEEK_SET);
							write(fd, &new_sec_rem_block, sizeof(int));
						}

						read(fdex, &buffer, BLOCK_SIZE);
						lseek(fd, new_ind_block*BLOCK_SIZE, SEEK_SET);
						write(fd, &buffer, BLOCK_SIZE);

						offset2 = offset2 + sizeof(int);
					}
				}
				offset = lseek(fd, offset + sizeof(int), SEEK_SET);
				offset2 = offset;

				j++;
			}
			if (j == sec_ind_blocks - 1)
			{
				for (j = sec_ind_blocks; j < 256; j++)
				{
					int zero = 0;
					lseek(fd, offset, SEEK_SET);
					write(fd, &zero, sizeof(int));
					offset = offset + sizeof(int);
				}
			}
		}

		i++;
	}

	writeToInode(new_Inode, inode_num);

	//printf("NUMBER OF BLOCKS: %d", blk_count);

	close(fdex);
}

/************** Function to copy file contents from external file to mv6 filesystem************/
void cpout(const char *ext_file, const char *sys_file, unsigned short inode_num, char *pathname)
{
	//For displaying pathname
	sys_file = strtok(pathname, " ");
	sys_file = strtok(NULL, " ");

	inode file_inode;
	unsigned short flag;
	lseek(fd, 2*BLOCK_SIZE + (inode_num-1)*I_SIZE, SEEK_SET);
	read(fd, &file_inode, sizeof(inode));

	//shift to get value of file type flag
	flag = file_inode.flags;
	flag = flag << 1;
	flag = flag >> 13;

	if (flag == 0)
	{
		printf("Copying small system file '%s' to external file '%s'\n", sys_file, ext_file);
		cpout_plainfile(ext_file, sys_file, file_inode);
	}
	else if (flag == 1)
	{
		printf("Copying large system file '%s' to external file '%s'\n", sys_file, ext_file);
		cpout_largefile(ext_file, sys_file, file_inode);
	}
}

void cpout_plainfile(const char *ext_file, const char *sys_file, inode file_inode)
{
	char buff[BLOCK_SIZE];
	int i = 0;
	int fdex;

	fdex = open(ext_file, O_WRONLY | O_CREAT, 0600);

	//copy to external file
	while(file_inode.addr[i] != 0)
	{
		lseek(fd, file_inode.addr[i] * BLOCK_SIZE, SEEK_SET);
		read(fd, &buff, BLOCK_SIZE);

		write(fdex, &buff, BLOCK_SIZE);
		i++;
	}
	close(fdex);
}

void cpout_largefile(const char *ext_file, const char *sys_file, inode file_inode)
{
	char buff[BLOCK_SIZE];
	int i = 0, j = 0, k = 0;
	int fdex;
	int offset, offset2, offset3;
	int block_num, block_num2;	//to hold block number

	int blk_count, blk2_count, blk3_count;

	fdex = open(ext_file, O_WRONLY | O_CREAT, 0600);

	//copy to external file
	while (file_inode.addr[i] != 0)
	{
		if (i < 7)	//for large file
		{
			offset = lseek(fd, file_inode.addr[i] * BLOCK_SIZE, SEEK_SET);	//read the first block_num in the indirect block
			blk_count++;
			read(fd, &block_num, sizeof(int));

			while (j < 256)
			{
				if (block_num == NULL || block_num == 0)
					break;
				else
				{
					//copy from system file to external file
					lseek(fd, block_num*BLOCK_SIZE, SEEK_SET);
					read(fd, &buff, BLOCK_SIZE);
					write(fdex, &buff, BLOCK_SIZE);

					//move to next entry in the indirect block
					offset = lseek(fd, offset + sizeof(int), SEEK_SET);
					read(fd, &block_num, sizeof(int));
					j++;
				}
				blk2_count++;
			}
			j = 0;
		}
		else if (i == 7)	//extra large file
		{
			offset = lseek(fd, file_inode.addr[i] * BLOCK_SIZE, SEEK_SET);	//seek read the first block_num in the indirect block
			read(fd, &block_num, sizeof(int));

			blk_count++;
			while (j < 256)
			{
				if (block_num == NULL || block_num == 0)
					break;
				else
				{
					offset2 = lseek(fd, block_num * BLOCK_SIZE, SEEK_SET);	//seek read the first block_num2 in the indirect block
					read(fd, &block_num2, sizeof(int));
					while (k < 256)
					{
						if (block_num2 == NULL || block_num2 == 0)
							break;

						//copy from system file to external file
						else
						{
							lseek(fd, block_num2*BLOCK_SIZE, SEEK_SET);
							read(fd, &buff, BLOCK_SIZE);
							write(fdex, &buff, BLOCK_SIZE);

							//move to next entry in the indirect block
							offset2 = lseek(fd, offset2 + sizeof(int), SEEK_SET);
							read(fd, &block_num2, sizeof(int));
							k++;
						}
						blk3_count++;
					}
					k = 0;
				}
				offset = lseek(fd, offset + sizeof(int), SEEK_SET);
				read(fd, &block_num, sizeof(int));
				j++;
				blk2_count++;
			}
			j = 0;
		}
		i++;
		if (i > 7)
			break;
	}

	//printf("NUMBER OF BLK1: %d, NUMBER OF BLK2: %d, NUMBER OF BLK3: %d", blk_count, blk2_count, blk3_count);
	close(fdex);
}

void remove_file(const char *sys_file, unsigned short inode_num, unsigned short block_num)
{
	inode rmFile_inode;
	int i;
	char buff[BLOCK_SIZE];
	char nullEntry[16];
	char nullInode[32];
	dir file_to_be_removed;
	int offset;

	lseek(fd, 2 * BLOCK_SIZE + (inode_num - 1)*I_SIZE, SEEK_SET);		//get file-to-be-removed inode
	read(fd, &rmFile_inode, sizeof(inode));

	int match_count = 0;

	//remove file name from parent directory
	lseek(fd, block_num*BLOCK_SIZE, SEEK_SET);		//get parent block
	offset = lseek(fd, 0, SEEK_CUR);
	read(fd, &file_to_be_removed, sizeof(dir));

	while (file_to_be_removed.filename != NULL || file_to_be_removed.filename != 0)
	{
		//Determine if file name matches
		for (i = 0; i < strlen(sys_file); i++)
		{
			if (file_to_be_removed.filename[i] == sys_file[i])
				match_count++;
		}

		//If found, replace the file-to-be-removed entry
		if (match_count == strlen(sys_file))
		{
			lseek(fd, offset, SEEK_SET);
			write(fd, &nullEntry, sizeof(dir));
			break;
		}
		offset = lseek(fd, 0, SEEK_CUR);
		read(fd, &file_to_be_removed, sizeof(dir));
	}

	//get file-to-be-removed flag to check if small file or large file
	unsigned short flag;
	lseek(fd, 2 * BLOCK_SIZE + (inode_num - 1)*I_SIZE, SEEK_SET);
	read(fd, &rmFile_inode, sizeof(inode));
	flag = rmFile_inode.flags;
	flag = flag << 1;
	flag = flag >> 13;

	if (flag == 0)
	{
		printf("\nRemoving small file, '%s'...\n", sys_file);
		remove_plainfile(sys_file, inode_num, block_num, rmFile_inode);
	}
	else if (flag == 1)
	{
		printf("\nRemoving large file, '%s'...\n", sys_file);
		remove_largefile(sys_file, inode_num, block_num, rmFile_inode);
	}
	else
	{
		printf("Unable to remove: this is not a file\n");
		return;
	}

	//clear file's inode data
	lseek(fd, 2 * BLOCK_SIZE + (inode_num - 1)*I_SIZE, SEEK_SET);
	write(fd, &nullInode, sizeof(nullInode));

	lseek(fd, 2 * BLOCK_SIZE + (inode_num - 1)*I_SIZE, SEEK_SET);		//get file-to-be-removed inode
	read(fd, &rmFile_inode, sizeof(inode));
	//printf("%d", rmFile_inode.addr[0]);

	//deallocate file-to-be-removed inode
	deallocate_inode(inode_num);
	printf("File successfully removed.\n");
}

void remove_plainfile(const char *sys_file, unsigned short inode_num, unsigned short block_num, inode rmFile_inode)
{
	int i;
	char buff[BLOCK_SIZE];
	char nullEntry[16];
	char nullInode[32];
	dir file_to_be_removed;
	int offset;

	//NULL blocks for removing file data
	for (i = 0; i < BLOCK_SIZE; i++)
		buff[i] = '\0';

	for (i = 0; i < sizeof(nullEntry); i++)
		nullEntry[i] = '\0';

	for (i = 0; i < sizeof(nullInode); i++)
		nullInode[i] = '\0';

	i = 0;

	//clear file's datablock
	while (rmFile_inode.addr[i] != 0)
	{
		//clear data blocks
		lseek(fd, rmFile_inode.addr[i] * BLOCK_SIZE, SEEK_SET);
		write(fd, &buff, BLOCK_SIZE);

		//add block back to free array
		addFreeBlock(rmFile_inode.addr[i]);

		i++;
	}

}

void remove_largefile(const char *sys_file, unsigned short inode_num, unsigned short block_num, inode rmFile_inode)
{
	int i, j, k;
	char buff[BLOCK_SIZE];
	char nullEntry[16];
	char nullInode[32];
	dir file_to_be_removed;
	int offset, offset2, offset3;
	int block_num2, block_num3;

	//NULL blocks for removing file data
	for (i = 0; i < BLOCK_SIZE; i++)
		buff[i] = '\0';

	for (i = 0; i < sizeof(nullEntry); i++)
		nullEntry[i] = '\0';

	for (i = 0; i < sizeof(nullInode); i++)
		nullInode[i] = '\0';

	i = 0;

	//clear file's datablock and indirect blocks
	while (rmFile_inode.addr[i] != 0)
	{
		if (i < 7)	//for large file
		{
			offset = lseek(fd, rmFile_inode.addr[i] * BLOCK_SIZE, SEEK_SET);	//read the first block_num in the indirect block
			read(fd, &block_num, sizeof(int));

			while (j < 256)
			{
				if (block_num == NULL || block_num == 0)
					break;
				else
				{
					lseek(fd, block_num*BLOCK_SIZE, SEEK_SET);

					//clear datablock
					write(fd, &buff, BLOCK_SIZE);

					//add block back to free array
					addFreeBlock(block_num);

					//move to next entry in the indirect block
					offset = lseek(fd, offset + sizeof(int), SEEK_SET);
					read(fd, &block_num, sizeof(int));
					j++;
				}
			}

			//clear indirect block
			lseek(fd, rmFile_inode.addr[i] * BLOCK_SIZE, SEEK_SET);
			write(fd, &buff, BLOCK_SIZE);

			//add block back to free array
			addFreeBlock(rmFile_inode.addr[i]);

			j = 0;
		}
		else if (i == 7)	//extra large file
		{
			offset = lseek(fd, rmFile_inode.addr[i] * BLOCK_SIZE, SEEK_SET);	//seek read the first block_num in the indirect block
			read(fd, &block_num, sizeof(int));

			while (j < 256)
			{
				if (block_num == NULL || block_num == 0)
					break;
				else
				{
					offset2 = lseek(fd, block_num * BLOCK_SIZE, SEEK_SET);	//seek read the first block_num2 in the indirect block
					read(fd, &block_num2, sizeof(int));
					while (k < 256)
					{
						if (block_num3 == NULL || block_num2 == 0)
							break;
						//copy from system file to external file
						else
						{
							lseek(fd, block_num2*BLOCK_SIZE, SEEK_SET);

							//clear datablock
							write(fd, &buff, BLOCK_SIZE);

							//add block back to free array
							addFreeBlock(block_num2);

							//move to next entry in the indirect block
							offset2 = lseek(fd, offset2 + sizeof(int), SEEK_SET);
							read(fd, &block_num2, sizeof(int));
							k++;
						}
					}

					//clear second indirect block
					lseek(fd, block_num * BLOCK_SIZE, SEEK_SET);
					write(fd, &buff, BLOCK_SIZE);

					//add block back to free array
					addFreeBlock(block_num);

					k = 0;
				}

				offset = lseek(fd, offset + sizeof(int), SEEK_SET);
				read(fd, &block_num, sizeof(int));
				j++;
			}

			//clear first indirect block
			lseek(fd, rmFile_inode.addr[i] * BLOCK_SIZE, SEEK_SET);
			write(fd, &buff, BLOCK_SIZE);

			//add block back to free array
			addFreeBlock(rmFile_inode.addr[i]);
			j = 0;
		}
		i++;
	}
}

/*************** Function to allocate inode ****************/
unsigned short allocate_inode()
{
	unsigned short inumber;
	unsigned int i = 0;
	super.ninode--;
	inumber = super.inode[super.ninode];
	return inumber;
}

/*************** Function to deallocate inode****************/
unsigned short deallocate_inode(unsigned short inode_num)
{
	if (super.ninode < 100)
	{
		super.inode[super.ninode] = inode_num;
		super.ninode++;
	}
}

/*************** Function to find offset ****************/
int offset_set(int block)
{
	int offset = 0;
	return block * BLOCK_SIZE + offset;
}

/*************** Function to search and/or create a file ****************/
int searchCreateFile(const char *pathname, unsigned short block_num, int cp_option)
{
	int i = 0, j = 0;
	int offset = 0;
	inode inode1;
	dir dir1;
	dir parent;
	char buff[50];

	i = 0;

	lseek(fd, block_num * BLOCK_SIZE, SEEK_SET);  //go to root directory
	read(fd, &dir1, sizeof(dir)); //offset after 1st entry
	read(fd, &dir1, sizeof(dir)); //offset after 2nd entry
	offset = lseek(fd, 0, SEEK_CUR); //same offset
	read(fd, &dir1, sizeof(dir1)); //offset after 3rd entry

	parent = dir1;

	int match_count = 0;
	while (dir1.inode != NULL || dir1.inode != 0)
	{
		for (i = 0; i < strlen(pathname); i++)
		{
			if (dir1.filename[i] == pathname[i])
				match_count++;
		}

		if (match_count == strlen(pathname))
		{
			return dir1.inode;	//return existing inode
		}

		offset = lseek(fd, 0, SEEK_CUR);
		read(fd, &dir1, sizeof(dir1)); //reads the next entries starting with 4th
	}

	// for cpout call
	if (cp_option == 1)
		return -1;

	//Store directory entry into the parent block
	dir dir2;

	int inum = allocate_inode();

	dir2.inode = inum;
	for (j = 0; j < 14; j++)
	{
		dir2.filename[j] = pathname[j];
	}

	lseek(fd, offset, SEEK_SET); //offset is where we left off at the parent block
	write(fd, &dir2, 16);  //offset is at end of created entry

	//int block = addFreeBlock();

	//setting inode structure for for new directory
	inode new_Inode;

	new_Inode.flags = inode_alloc | pfile | 000007;   		// flag for root directory
	new_Inode.nlinks = 2;
	new_Inode.uid = '0';
	new_Inode.gid = '0';
	new_Inode.size0 = '0';
	new_Inode.size1 = I_SIZE;
	new_Inode.addr[0] = 0;
	for (i = 1; i < 8; i++)
		new_Inode.addr[i] = 0;

	new_Inode.actime[0] = 0;
	new_Inode.actime[1] = 0;
	new_Inode.modtime[0] = 0;
	new_Inode.modtime[1] = 0;

	writeToInode(new_Inode, inum);
	return inum; //return newly allocated inode num
}