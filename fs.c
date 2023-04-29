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

// Global Variables
extern struct disk *thedisk;
int is_mounted = 0;
int *free_bit_map = NULL;

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

	int n = disk_nblocks(thedisk) / 10;
	if ((disk_nblocks(thedisk) % 10) != 0)
	{
		n++;
	}

	int n_inodes_blocks = n;

	union fs_block b;
	b.super.magic = FS_MAGIC;
	b.super.nblocks = disk_nblocks(thedisk);
	b.super.ninodeblocks = n_inodes_blocks;
	b.super.ninodes = n_inodes_blocks * INODES_PER_BLOCK;
	disk_write(thedisk, 0, b.data);
	int to = b.super.ninodeblocks;
	// iterate through inodes and init it to 0
	for (int i = 1; i < to; i++)
	{
		memset(b.inode, 0, sizeof(struct fs_inode));
		disk_write(thedisk, i, b.data);
	}

	// we allocated just the superblock
	return 1;
}
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

	for (int i = 0; i < inode_blocks - 1; i++)
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
	union fs_block super_block;
	disk_read(thedisk, 0, super_block.data);

	if (super_block.super.magic != FS_MAGIC)
	{
		// pemar
		return 0;
	}
	if (super_block.super.ninodes == 0 || super_block.super.nblocks == 0)
	{
		// pemar
		return 0;
	}

	free_bit_map = (int *)calloc(super_block.super.nblocks, sizeof(int));
	if (!free_bit_map)
	{
		exit(1);
	}
	int NFB = super_block.super.nblocks;
	// super block

	// scan through filesystem and mark what is in use

	for (int i = 0; i < super_block.super.ninodeblocks; i++)
	{
		free_bit_map[i] = 1;
		NFB--;
	}

	for (int i = 1; i < super_block.super.ninodeblocks; i++)
	{
		/* code */
		union fs_block b;
		disk_read(thedisk, i, b.data);
		// iterate through each inode in the block
		for (int j = 0; j < INODES_PER_BLOCK; j++)
		{
			if (!b.inode[j].isvalid)
			{
				continue;
			}

			for (int k = 0; k < POINTERS_PER_INODE; k++)
			{
				if (b.inode[j].direct[k])
				{
					free_bit_map[b.inode[j].direct[k]] = 1;
					NFB--;
				}
			}
			if (b.inode[j].indirect != 0)
			{
				// iterate through all the indirect
				union fs_block indirect_block;
				disk_read(thedisk, b.inode[j].indirect, indirect_block.data);
				for (int k = 0; k < POINTERS_PER_BLOCK; k++)
				{

					if (indirect_block.pointers[k])
					{
						free_bit_map[indirect_block.pointers[k]] = 1;
						NFB--;
					}
					else
					{
						break;
					}
				}
			}
		}
	}

	is_mounted = 1 == 1;

	return 1;
}
int fs_create()
{
	// Create a new inode of zero length. On success, return the (positive)
	// inumber. On failure, return zero.

	if (!is_mounted)
	{
		return 0;
	}

	// Read super_block and get number of inodes
	union fs_block block;
	disk_read(thedisk, 0, block.data);

	int NIN = block.super.ninodes;
	int BLK = 1;
	int INODEI = 0;

	union fs_block b;
	disk_read(thedisk, BLK, b.data);

	for (int i = 1; i < NIN - 1; i++)
	{
		/* code */

		if (i % INODES_PER_BLOCK == 0)
		{
			BLK += 1;
			disk_read(thedisk, BLK, b.data);
		}

		if (b.inode[i].isvalid == 0)
		{
			INODEI = i;
			break;
		}
		// iterate through each inode in the block
	}

	if (!INODEI)
	{
		return 0;
	}

	b.inode[INODEI].isvalid = 1;

	// set ctime
	b.inode[INODEI].ctime = time(NULL);
	b.inode[INODEI].direct[0] = 0;
	b.inode[INODEI].direct[1] = 0;
	b.inode[INODEI].direct[2] = 0;
	b.inode[INODEI].indirect = 0;

	disk_write(thedisk, BLK, b.data);

	return INODEI;

	return 0;
}
int fs_delete(int inumber)
{
	// Delete the inode indicated by the inumber. Release all data and indirect
	// blocks assigned to this inode and return them to the free block map. On
	// success, return one. On failure, return 0.

	if (!is_mounted)
	{
		abort();
		return 0;
	}
	if (inumber < 1)
	{
		abort();
		return 0;
	}

	int BLK = inumber / INODES_PER_BLOCK + 1;
	// if (inumber % INODES_PER_BLOCK == 0)
	// {
	// 	BLK -= 1;
	// }
	int OFF = inumber % INODES_PER_BLOCK;

	union fs_block b;
	disk_read(thedisk, BLK, b.data);

	if (!b.inode[OFF].isvalid)
	{
		return 0;
	}

	// Redundant (done below)
	//  b.inode[OFF].isvalid = 0;
	//  b.inode[OFF].ctime = 0;

	for (int i; i < POINTERS_PER_INODE; i++)
	{
		if (b.inode[OFF].direct[i])
		{
			free_bit_map[b.inode[OFF].direct[i]] = 0;
		}
		b.inode[OFF].direct[i] = 0;
	}

	if (b.inode[OFF].indirect)
	{
		union fs_block indirect_block;
		disk_read(thedisk, b.inode[OFF].indirect, indirect_block.data);
		for (int i = 0; i < POINTERS_PER_BLOCK; i++)
		{
			if (indirect_block.pointers[i])
			{
				free_bit_map[indirect_block.pointers[i]] = 0;
				// set the indirect pointer to all zeros

				indirect_block.pointers[i] = 0;
			}
		}

		// free_bit_map[b.inode[OFF].indirect] = 0;
		b.inode[OFF].indirect = 0;
		disk_write(thedisk, b.inode[OFF].indirect, indirect_block.data);

		memset(&b.inode[OFF], 0, sizeof(struct fs_inode));
	}

	disk_write(thedisk, BLK, b.data);

	return 1;
}
int fs_getsize(int inumber)
{
	// Return the logical size of the given inode, in bytes. Zero is a valid
	// logical size for an inode! On failure, return -1

	if (!is_mounted)
	{
		return -1;
	}

	int BLK = inumber / INODES_PER_BLOCK + 1;
	int OFF = inumber % INODES_PER_BLOCK;

	union fs_block b;

	disk_read(thedisk, BLK, b.data);

	if (!b.inode[OFF].isvalid)
	{
		return -1;
	}

	return b.inode[OFF].size;
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
