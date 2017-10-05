#include "ext2.h"

#define INODES_COUNT 32
#define BLOCKS_COUNT 128


int find_next_dir(char* query, char* buf);
int find_dir_inode(char* query, void* inodes);
int* get_inode_bitmap(void* start);
int* get_block_bitmap(void* start);
void set_inode_bitmap(void* start, int inode_idx, int bitval);
void set_block_bitmap(void* start, int block_idx, int bitval);
int* find_free_blocks(int* block_bitmap, int num);
int total_free_blocks(void* start);
int total_free_inodes(void* start);
void separate(char* path, char* name);
