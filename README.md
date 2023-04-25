# Project #6: SVSFS

This is a shared repository for the final project in the Operating Systems class. 

This project is co-authored: **Santiago Rodríguez** & **José Benítez**. 

## The goals of this project:
- Learn about the data structures and implementation of a Unix-like filesystem.
- Learn about filesystem recovery by implementing a free block bitmap scan.
- Develop expertise in C programming by using structures and unions extensively.

## Overview
We will build a file system called SVSFS, which is similar to the Unix inode layer. We'll start with a design description and then implement the code. To avoid damaging existing disks and filesystems, we'll use a disk emulator, a software that loads and stores data in blocks. To test our implementation, we'll use a shell and example file system images. 
