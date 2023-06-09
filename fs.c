#include "fs.h"
#include "disk.h"
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define FS_MAGIC 0x34341023
#define INODES_PER_BLOCK 128
#define POINTERS_PER_INODE 3
#define POINTERS_PER_BLOCK 1024
#define MIN(a, b) ((a) < (b) ? (a) : (b))

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
int *freeblock = NULL;



// Provided by Flynn:
int isfree(int b)
{
	int ix = b / 8; // 8 bits per byte; this is the byte number
	unsigned char mask[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
	// if bit is set, return nonzero; else return zero
	return ((freeblock[ix] & mask[b % 8]) != 0);
}

int getfreeblock()
{
	// printf("Searching %d blocks for a free block\n",disk_nblocks(thedisk));
	for (int i = 0; i < disk_nblocks(thedisk); i++)
		if (freeblock[i] == 1)
		{
			// printf("found free block at %d\n",i);
			return i;
		}
	printf("No free blocks found\n");
	return -1;
}

int getfblockindex(struct fs_inode *inode, unsigned int blkno)
{
	assert(blkno < (POINTERS_PER_BLOCK + POINTERS_PER_INODE));
	if (blkno < POINTERS_PER_INODE)
	{
		assert(inode->direct[blkno] != 0);

		assert(!isfree(inode->direct[blkno]));
#ifdef DEBUG
		printf("fblock %d, dblock %d (direct)\n", blkno, inode->direct[blkno]);
#endif
		return inode->direct[blkno];
	}
	else
	{
		// read indirect block
		union fs_block ib;
		assert(!isfree(inode->indirect));
		disk_read(thedisk, inode->indirect, ib.data);
		// read data pointed to by pointer within the block (make sure to offset index)
		int dbno = ib.pointers[blkno - POINTERS_PER_INODE];
#ifdef DEBUG
		printf("fblock %d, dblock %d (indirect in block %d)\n", blkno, dbno, inode->indirect);
#endif
		assert(dbno != 0);
		assert(!isfree(dbno));
		return dbno;
	}
}

void getfblock(struct fs_inode *inode, unsigned char *data, unsigned int fblkno, unsigned int *bkidx)
{
	int dblkno = getfblockindex(inode, fblkno);
#ifdef DEBUG
	printf("getfblock: file block %d is disk block %d\n", fblkno, dblkno);
#endif
	disk_read(thedisk, dblkno, data);
	if (bkidx)
		*bkidx = dblkno;
}

// set the bit indicating that block b is free.
void markfree(int b)
{
	int ix = b / 8; // 8 bits per byte; this is the index of the byte to modify
	unsigned char mask[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
	// OR in the bit that corresponds to the block we're talking about.
	freeblock[ix] |= mask[b % 8];
}

// set the bit indicating that block b is used.
void markused(int b)
{
	int ix = b / 8; // 8 bits per byte; this is the index of the byte to modify
	unsigned char mask[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
	// AND the byte with the inverse of the bitmask to force the desired bit to 0
	freeblock[ix] &= ~mask[b % 8];
}

int pemar(char *message)
{
	fprintf(stderr, "%s\n", message);
	return 0;
}

void print_freeblock(){
	// print free block
	for(int	i = 0; i < disk_nblocks(thedisk); i++){
		freeblock[i] ? printf("1") : printf("0");
	}
	printf("\n");
}

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
		return pemar("error: system is already mounted");
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
				if (block.inode[j].isvalid == 0)
				{
					printf("    valid: NO\n");
				}
				else
				{
					printf("    valid: YES\n");
				}
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
		return pemar("error: superblock does not match the MAGIC number.");
	}
	if (super_block.super.ninodes == 0 || super_block.super.nblocks == 0)
	{
		// pemar
		return pemar("error: the filesystem has no blocks");
	}

	// freeblock = (int *)malloc(super_block.super.nblocks, sizeof(unsigned));
	
	int NFB = 0;

	 // build free block bitmap
    if (freeblock) free(freeblock);
	freeblock = (int *)malloc(super_block.super.nblocks * sizeof(int));
	if (!freeblock)
	{
		exit(1);
	}
    
	// super block is not free
	freeblock[0] = 0;
	for (int i = 1; i < super_block.super.nblocks; i++) {
		freeblock[i] = 1;
		NFB++;
	}

	// scan through filesystem and mark what is in use

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
					markused(b.inode[j].direct[k]);
					freeblock[b.inode[j].direct[k]] = 0;
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
						markused(indirect_block.pointers[k]);
						freeblock[indirect_block.pointers[k]] = 0;
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
		return pemar("error: system is already mounted");
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
		return pemar("error: system is full and can't create more inodes.");
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
		return pemar("error: system is already mounted");
	}

	if (inumber < 1)
	{
		char error[100];
		sprintf(error, "error: invalid inode %d.", inumber);
		return pemar(error);
	}

	int BLK = inumber / INODES_PER_BLOCK + 1;
	int OFF = inumber % INODES_PER_BLOCK;

	union fs_block b;
	disk_read(thedisk, BLK, b.data);

	if (!b.inode[OFF].isvalid)
	{
		char error[100];
		sprintf(error, "error: invalid inode %d is already marked invalid.", inumber);
		return pemar(error);
	}

	for (int i = 0; i < POINTERS_PER_INODE; i++)
	{
		if (b.inode[OFF].direct[i])
		{
			// markfree(b.inode[OFF].direct[i]);
			freeblock[b.inode[OFF].direct[i]] = 1;
			b.inode[OFF].direct[i] = 0;
		}
	}

	if (b.inode[OFF].indirect)
	{
		union fs_block indirect_block;
		disk_read(thedisk, b.inode[OFF].indirect, indirect_block.data);
		for (int i = 0; i < POINTERS_PER_BLOCK; i++)
		{
			if (indirect_block.pointers[i])
			{
				// markfree(indirect_block.pointers[i]);
				freeblock[indirect_block.pointers[i]] = 1;
				indirect_block.pointers[i] = 0;
			}
		}

		// markfree(b.inode[OFF].indirect);
		freeblock[b.inode[OFF].indirect] = 1;
		b.inode[OFF].indirect = 0;
		disk_write(thedisk, b.inode[OFF].indirect, indirect_block.data);
	}

	memset(&b.inode[OFF], 0, sizeof(struct fs_inode));
	disk_write(thedisk, BLK, b.data);
	// markfree(BLK);
	freeblock[BLK] = 1;

	return 1;
}

int fs_getsize(int inumber)
{
	// Return the logical size of the given inode, in bytes. Zero is a valid
	// logical size for an inode! On failure, return -1

	if (!is_mounted)
	{
		return pemar("error: system is already mounted") - 1;
	}

	int BLK = inumber / INODES_PER_BLOCK + 1;
	int OFF = inumber % INODES_PER_BLOCK;

	union fs_block b;

	disk_read(thedisk, BLK, b.data);

	if (!b.inode[OFF].isvalid)
	{
		char error[100];
		sprintf(error, "error: inode %d is invalid.", inumber);
		return pemar(error) - 1;
	}

	return b.inode[OFF].size;
}

int bytes_to_read(union fs_block block, int length, int offset)
{
	// End Case
	if (offset < length && length < offset + BLOCK_SIZE)
	{
		return length - offset;
	}

	// Partial and full block cases
	int BR = BLOCK_SIZE - (offset % BLOCK_SIZE);

	return BR; // return total bytes
}

int fs_read(int inumber, unsigned char *data, int length, int offset)
{
	/** Read data from a valid inode. Copy length bytes from the inode into the
  address pointed to by data, starting at offset in the inode. Return the total
  number of bytes read. The number of bytes actually read could be smaller than
  the number of bytes requested, perhaps if the end of the inode is reached. If
  the given inumber is invalid, or any other error is encountered, return 0.
  **/

	// 1. If file system has not been mounted, PEMAR
	if (!is_mounted)
	{
		return pemar("error: system is already mounted");
	}

	// 2. figure out block number BLK and offset OFF of inode numbered inumber
	int BLK = inumber / INODES_PER_BLOCK + 1;
	int OFF = inumber % INODES_PER_BLOCK;

	// 3. read BLK and look at the inode INODE corresponding to index `inumber` (use OFFto get the index)
	union fs_block block;
	disk_read(thedisk, BLK, block.data);
	struct fs_inode inode = block.inode[OFF];

	// 3a. If INODE is not valid, PEMAR
	if (inode.isvalid == 0)
	{
		return pemar("error: inode is not valid");
	}

	// 3b. If offset is > file size, PEMAR
	if (offset > inode.size)
	{
		// at the end of the file
		return pemar("error: offset is greater than inode size");
	}

	// 3c. If offset + length > file size, reduce length to size - offset
	if (offset + length > inode.size)
	{
		length = inode.size - offset;
	}

	// 4. Read length bytes from the blocks on disk and copy them to data
	// 4a Read a whole block

	int changing_blk = offset / BLOCK_SIZE;				   // inode block number
	int changing_off = offset - changing_blk * BLOCK_SIZE; // offset in the block

	int bytes_read = 0;
	union fs_block buffer_block;

	while (bytes_read < length)
	{
		int BLK = getfblockindex(&inode, offset + bytes_read);
		disk_read(thedisk, BLK, buffer_block.data);

		int BTR = bytes_to_read(buffer_block, length, offset + bytes_read);
		memcpy(data + bytes_read, buffer_block.data + changing_off, BTR);
		bytes_read += BTR;
	}

	return bytes_read;
}

int allocate_block(int old_file_size, struct fs_inode * INODE, int blocks_to_allocate)
{
	// Loop through the direct blocks
	// If there is a free block, return it
	// If there is no free block, allocate a new block

	int blocks_allocated = 0;
	for (int i = 0; i < POINTERS_PER_INODE && blocks_allocated < blocks_to_allocate; i++)
	{
		if (!INODE->direct[i])
		{
			int new_block = getfreeblock();
			if (new_block == -1)
			{
				break;
			}
			INODE->direct[i] = new_block; 	
			//markused(new_block);
			freeblock[new_block] = 0;
			blocks_allocated++;
		}
	}

	if (blocks_allocated < blocks_to_allocate)
	{
		if (INODE->indirect == 0)
		{
			// allocate a new block in inode
			int new_block = getfreeblock();
			if (new_block == -1)
			{
				blocks_to_allocate = blocks_allocated;
			}
			else
			{
				INODE->indirect = new_block;
				// allocate a new block in free block map
				//markused(new_block);
				freeblock[new_block] = 0;
				union fs_block indirect_block;
				memset(indirect_block.data, 0, BLOCK_SIZE);
				disk_write(thedisk, INODE->indirect, indirect_block.data);
			}
		}

		union fs_block indirect_block;
		disk_read(thedisk, INODE->indirect, indirect_block.data);

		// Loop through the indirect blocks
		for (int i = 0; i < POINTERS_PER_BLOCK; i++)
		{
			if (indirect_block.pointers[i] == 0)
			{
				int new_block = getfreeblock();

				// no blocks available
				if (new_block == -1)
				{
					break;
				}
				indirect_block.pointers[i] = new_block;

				// allocate a new block in inode
				//markused(new_block);
				freeblock[new_block] = 0;

				// increase the number of blocks to allocate
				blocks_allocated++;

				if (blocks_allocated >= blocks_to_allocate)
				{
					break;
				}
			}
		}
		disk_write(thedisk, INODE->indirect, indirect_block.data);
	}
	return blocks_allocated;
}

int fs_write(int inumber, const unsigned char *data, int length, int offset)
{
	// 1.If file system has not been mounted, PEMAR
	if (!is_mounted)
	{
		return pemar("Error: system is not mounted");
	}

	// 2.Figure out block number BLK and offset OFF of inode numbered inumber
	int BLK = inumber / INODES_PER_BLOCK + 1;
	int OFF = inumber % INODES_PER_BLOCK;

	union fs_block block;
	disk_read(thedisk, BLK, block.data);
	struct fs_inode INODE = block.inode[OFF];

	// Read BLK and look at the inode INODE corresponding to index inumber (use OFF to get the index)
	// If INODE is not valid, PEMAR
	if (!(INODE.isvalid))
	{
		char error[100];
		sprintf(error, "Error: inode %d is not valid", inumber);
		return pemar(error);
	}

	int new_file_size = offset + length;
	int old_file_size = INODE.size;
	int new_num_blocks = (new_file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	int old_num_blocks = (old_file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	int blocks_to_allocate = new_num_blocks - old_num_blocks;

	// Allocate new blocks
	if (blocks_to_allocate)
	{
		allocate_block(old_file_size, &INODE, blocks_to_allocate);
	}

	int bytes_written = 0;
	int remaining_length = length;
	int current_block = offset / BLOCK_SIZE;
	int changing_off = offset % BLOCK_SIZE;

	while (bytes_written < length)
	{

		int BTW = MIN(BLOCK_SIZE - changing_off, remaining_length);
		union fs_block buffer_block;
		int useBLK = getfblockindex(&INODE, current_block);

		if (BTW != BLOCK_SIZE)
		{
			disk_read(thedisk, useBLK, buffer_block.data);
		}

		// Write to buffer block
		memcpy(buffer_block.data + changing_off, data + bytes_written, BTW);

		// Write buffer block to disk
		disk_write(thedisk, useBLK, buffer_block.data);

		bytes_written += BTW;
		remaining_length -= BTW;
		current_block++;
		changing_off = 0;
	}

	if (INODE.size < new_file_size)
	{
		INODE.size = new_file_size;
		disk_write(thedisk, BLK, block.data);
	}

	return bytes_written;
}