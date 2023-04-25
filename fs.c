/*
 * SVSFS - a close copy of D. Thain's SimpleFS.
 * PJF 4/2023
 * original comments below
 * Implementation of SimpleFS.
 * Make your changes here.
*/

#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdint.h>

extern struct disk *thedisk;

#define FS_MAGIC           0x34341023
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 3
#define POINTERS_PER_BLOCK 1024

struct fs_superblock {
	uint32_t magic;
	uint32_t nblocks;
	uint32_t ninodeblocks;
	uint32_t ninodes;
};

struct fs_inode {
	uint32_t isvalid;
	uint32_t size;
	int64_t ctime;
	uint32_t direct[POINTERS_PER_INODE];
	uint32_t indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	unsigned char data[BLOCK_SIZE];
};

int fs_format()
{
	return 0;
}

void fs_debug()
{
	union fs_block block;

	disk_read(thedisk,0,block.data);

	printf("superblock:\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);
}

int fs_mount()
{
	return 0;
}

int fs_create()
{
	return 0;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return 0;
}

int fs_read( int inumber, unsigned char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const unsigned char *data, int length, int offset )
{
	return 0;
}
