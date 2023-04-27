#include "fs.h"
#include "disk.h"
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FS_MAGIC 0x34341023
#define INODES_PER_BLOCK 128
#define POINTERS_PER_INODE 3
#define POINTERS_PER_BLOCK 1024

// Global Variables
union fs_block *svsfs = NULL;
extern struct disk *thedisk;
int is_mounted = 0;
int *free_bit_map = NULL;

struct fs_superblock
{
	uint32_t magic;
	uint32_t nblocks;
	uint32_t ninodeblocks;
	uint32_t ninodes;
};

struct fs_inode
{
	uint32_t isvalid;
	uint32_t size;
	int64_t ctime;
	uint32_t direct[POINTERS_PER_INODE];
	uint32_t indirect;
};

union fs_block
{
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	unsigned char data[BLOCK_SIZE];
};

int fs_format()
{
	/**
	Creates a new filesystem on the disk, destroying any data already present.
	Sets aside ten percent of the blocks for inodes, clears the inode table, and
	writes the superblock. Returns one on success, zero otherwise. Formatting a
	filesystem does not cause it to be mounted. Also, an attempt to format an
	already-mounted disk should do nothing and return failure.
	**/
	if (is_mounted)
	{
		return 0;
	}

	if (svsfs)
	{
		for (int i = 0; i < svsfs[0].super.nblocks; i++)
		{
			if (&svsfs[i] == NULL)
			{
				continue;
			}
			free(&svsfs[i]);
		}
		// free the fs
		free(svsfs);
	}

	svsfs = (union fs_block *)malloc(sizeof(union fs_block) * disk_nblocks(thedisk));
	if (!svsfs)
	{
		return 0;
	}

	int n_inodes_blocks = disk_nblocks(thedisk) / 10;

	union fs_block *superblock_temp = malloc(sizeof(union fs_block));
	if (!superblock_temp)
	{
		return 0;
	}
	superblock_temp->super.magic = FS_MAGIC;
	superblock_temp->super.nblocks = disk_nblocks(thedisk);
	superblock_temp->super.ninodeblocks = n_inodes_blocks;
	superblock_temp->super.ninodes = n_inodes_blocks * INODES_PER_BLOCK;
	svsfs[0] = *superblock_temp;
	free(superblock_temp);

	for (int i = 1; i <= n_inodes_blocks; i++)
	{
		union fs_block *inode_block_temp = malloc(sizeof(union fs_block));
		if (!inode_block_temp)
		{
			return 0;
		}
		memset(inode_block_temp, 0, sizeof(union fs_block));
		svsfs[i] = *inode_block_temp;
		free(inode_block_temp);
	}

	return 1;
}

#include <time.h>

void fs_debug()
{
	/**
	Scan a mounted filesystem and report on how the inodes and blocks are organized.
	**/
	union fs_block block;

	disk_read(thedisk, 0, block.data);

	printf("superblock:\n");
	printf("    %d blocks\n", block.super.nblocks);
	printf("    %d inode blocks\n", block.super.ninodeblocks);
	printf("    %d inodes\n", block.super.ninodes);

	int ninodes = block.super.ninodes;
	int inodes_per_block = BLOCK_SIZE / sizeof(struct fs_inode);
	int inode_blocks = ninodes / inodes_per_block;

	for (int i = -1; i < inode_blocks - 1; i++)
	{
		// printf("\n\nRound %d\n\n", i+1);
		disk_read(thedisk, i + 1, block.data);
		for (int j = 0; j < inodes_per_block; j++)
		{
			if (block.inode[j].isvalid)
			{
				char ctime_str[30];
				struct tm *ctime_tm = localtime(&block.inode[j].ctime);
				strftime(ctime_str, sizeof(ctime_str), "%a %b %d %H:%M:%S %Y", ctime_tm);

				printf("inode %d:\n", j);
				printf("    size: %d bytes\n", block.inode[j].size);
				printf("    created: %s\n", ctime_str);
				printf("    direct blocks:");
				for (int k = 0; k < POINTERS_PER_INODE; k++)
				{
					if (block.inode[j].direct[k])
					{
						printf(" %d", block.inode[j].direct[k]);
					}
				}
				printf("\n");
				if (block.inode[j].indirect)
				{
					printf("    indirect block: %d\n", block.inode[j].indirect);
					printf("    indirect data blocks:");
					disk_read(thedisk, block.inode[j].indirect, block.data);
					for (int k = 0; k < POINTERS_PER_BLOCK; k++)
					{
						if (block.pointers[k])
						{
							printf(" %d", block.pointers[k]);
						}
					}
					printf("\n");
				}
			}
		}
	}

	printf("\n");

	return;
}

int fs_mount()
{
	/**
	Examine the disk for a filesystem. If one is present, read the superblock,
	build a free block bitmap, and prepare the filesystem for use. Return one on success, zero otherwise.
	A successful mount is a pre-requisite for the remaining calls.
	**/
	if (!svsfs || svsfs[0].super.magic != FS_MAGIC)
	{
		return 0;
	}
	free_bit_map = (int *)malloc(sizeof(int) * svsfs[0].super.nblocks);
	if (!free_bit_map)
	{
		exit(1);
	}
	for (int i = 0; i < svsfs[0].super.nblocks; i++)
	{
		free_bit_map[i] = 0;
	}

	is_mounted = 1 == 1;

	return 0;
}

int fs_create()
{
	// Create a new inode of zero length. On success, return the (positive)
	// inumber. On failure, return zero.

	int new_inode = 0;
	for (int i = 1; i < svsfs->super.ninodeblocks; i++)
	{
		if (free_bit_map[i] == 0 && svsfs[i].inode == 0)
		{
			/* code */
			union fs_block *new_block = malloc(sizeof(union fs_block));
			if (!new_block)
			{
				return 0;
			}
			time_t curtime;
			time(&curtime);
			new_block->inode->ctime = ctime(&curtime);
			new_block->inode->size = 0;
			new_block->inode->indirect = 0;
			svsfs[i] = *new_block;
			return new_inode;
		}
	}

	return new_inode;
}

int fs_delete(int inumber)
{
	// Delete the inode indicated by the inumber. Release all data and indirect
	// blocks assigned to this inode and return them to the free block map. On
	// success, return one. On failure, return 0.
	return 0;
}

int fs_getsize(int inumber)
{
	// Return the logical size of the given inode, in bytes. Zero is a valid
	// logical size for an inode! On failure, return -1
	return 0;
}

int fs_read(int inumber, unsigned char *data, int length, int offset)
{
	/** Read data from a valid inode. Copy length bytes from the inode into the
  address pointed to by data, starting at offset in the inode. Return the total
  number of bytes read. The number of bytes actually read could be smaller than
  the number of bytes requested, perhaps if the end of the inode is reached. If
  the given inumber is invalid, or any other error is encountered, return 0.
  **/
	return 0;
}

int fs_write(int inumber, const unsigned char *data, int length, int offset)
{
	/**
  Write data to a valid inode. Copy length bytes from the address pointed to by
  data into the inode starting at offset bytes. You may need to allocate new
  direct and indirect blocks to store this written data.Return the number of
  bytes actually written. The number of bytes actually written could be smaller
  than the number of bytes request, perhaps if the disk becomes full. If the
  given inumber is invalid, or any other error is encountered, return 0.
  **/
	return 0;
}
