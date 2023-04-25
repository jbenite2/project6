/*
 * SVSFS - a close copy of D. Thain's SimpleFS.
 * PJF 4/2023
 * original comments below
 * Implementation of SimpleFS.
 * Make your changes here.
 */

#include "fs.h"
#include "disk.h"

#include <stdint.h>
#include <stdio.h>

extern struct disk *thedisk;

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

int fs_format() { return 0; }

void fs_debug()
{
	/**
      Scan a mounted filesystem and report on how the inodes and blocks are
  organized.
  **/
	union fs_block block;

	disk_read(thedisk, 0, block.data);

	printf("superblock:\n");
	printf("    %d blocks\n", block.super.nblocks);
	printf("    %d inode blocks\n", block.super.ninodeblocks);
	printf("    %d inodes\n", block.super.ninodes);
}

int fs_mount()
{
	/**
   Creates a new filesystem on the disk, destroying any data already present.
  Sets aside ten percent of the blocks for inodes, clears the inode table, and
  writes the superblock. Returns one on success, zero otherwise. Formatting a
  filesystem does not cause it to be mounted. Also, an attempt to format an
  already-mounted disk should do nothing and return failure.
  **/
	return 0;
}

int fs_create()
{
	// Create a new inode of zero length. On success, return the (positive)
	// inumber. On failure, return zero.
	return 0;
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
