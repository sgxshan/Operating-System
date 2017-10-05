#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util.h"

#define INODES_COUNT 32
#define BLOCKS_COUNT 128

extern unsigned char* disk;


int find_next_dir(char* query, char* buf) {
  if (query[0] != '/') {
    return -1;
  }
  int i, j;
  for (i = 1, j = 0; query[i] != '/'; i++, j++) {
    if (query[i] == '\0') {
      break;
    }
    buf[j] = query[i];
  }
  buf[j] = '\0';
  return i;
}

int find_dir_inode(char* query, void* inodes) {

  int inode_num = EXT2_ROOT_INO; // inode 2 root dir
  int block_num;
  struct ext2_inode* inode;
  struct ext2_dir_entry_2* dir;
  char name_buf[512] = "undefined"; // enough, maybe a bit too big

  while (1) {

    inode = (struct ext2_inode*)
      (inodes + (inode_num - 1) * sizeof(struct ext2_inode));
    int total_size = inode->i_size;

    // printf("next: %s name: %s\n", query, name_buf);

    // hard set "" to represent "/"
    if (strcmp(query, "") == 0) {
      query[0] = '/';
      query[1] = '\0';
    }

    if (strcmp(query, "/") == 0) {
      return inode_num - 1;
    }

    int n = find_next_dir(query, name_buf);
    query += n;

    int curr_size = 0;
    int new_inode_num = -1;
    int block_num_idx = 0;
    block_num = inode->i_block[block_num_idx];
    dir = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
    while (curr_size < total_size) {
      curr_size += dir->rec_len;
      if (dir->file_type == EXT2_FT_DIR) {
        if (strlen(name_buf) == dir->name_len &&
            strncmp(name_buf, dir->name, dir->name_len) == 0) {
          new_inode_num = dir->inode;
          break;
        }
      }
      dir = (void*)dir + dir->rec_len;
      if (curr_size % EXT2_BLOCK_SIZE == 0) { // need to use next pointer
        block_num_idx++;
        block_num = inode->i_block[block_num_idx];
        dir = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
      }
    }

    if (new_inode_num == -1) {
      // printf("not found: %s\n", name_buf);
      return -1;
    } else {
      inode_num = new_inode_num;
    }

  }
}

int* get_inode_bitmap(void* start) {
  int* inode_bitmap = malloc(sizeof(int) * INODES_COUNT);
  int i, byte_off, bit_off;
  for (i = 0; i < INODES_COUNT; i++) {
    byte_off = i / 8;
    bit_off = i % 8;
    char* byte = start + byte_off;
    inode_bitmap[i] = (*byte >> bit_off) & 1;
  }
  return inode_bitmap;
}

int* get_block_bitmap(void* start) {
  int* block_bitmap = malloc(sizeof(int) * BLOCKS_COUNT);
  int i, byte_off, bit_off;
  for (i = 0; i < BLOCKS_COUNT; i++) {
    byte_off = i / 8;
    bit_off = i % 8;
    char* byte = start + byte_off;
    block_bitmap[i] = (*byte >> bit_off) & 1;
  }
  return block_bitmap;
}

void set_inode_bitmap(void* start, int inode_idx, int bitval) {
  int byte_off = inode_idx / 8;
  int bit_off = inode_idx % 8;
  char* byte = start + byte_off;
  *byte = (*byte & ~(1 << bit_off)) | (bitval << bit_off);
}

void set_block_bitmap(void* start, int block_idx, int bitval) {
  int byte_off = block_idx / 8;
  int bit_off = block_idx % 8;
  char* byte = start + byte_off;
  *byte = (*byte & ~(1 << bit_off)) | (bitval << bit_off);
}

int* find_free_blocks(int* block_bm, int num) {

  int i = 0, j = 0;
  int* blocks = malloc(sizeof(int) * num);
  while (i < num) {
    while (block_bm[j] == 1) {
      j++;
      if (j == BLOCKS_COUNT) {
        return NULL;
      }
    }
    blocks[i] = j;
    i++;
    j++;
  }
  return blocks;
}

int total_free_blocks(void* start) {
  int i, byte_off, bit_off;
  char* byte;
  int count = 0;
  for (i = 0; i < BLOCKS_COUNT; i++) {
    byte_off = i / 8;
    bit_off = i % 8;
    byte = start + byte_off;
    count += (*byte >> bit_off) & 1;
  }
  return BLOCKS_COUNT - count;
}

int total_free_inodes(void* start) {
  int i, byte_off, bit_off;
  char* byte;
  int count = 0;
  for (i = 0; i < INODES_COUNT; i++) {
    byte_off = i / 8;
    bit_off = i % 8;
    byte = start + byte_off;
    count += (*byte >> bit_off) & 1;
  }
  return INODES_COUNT - count;
}

// separate path to last dir and filename
// /level1/level2 -> /level1 and level2
void separate(char* path, char* name) {
  int raw_len = strlen(path);
  int i = raw_len - 1;
  while (path[i] != '/') {
    i--;
  }
  int name_len = raw_len - i - 1;
  strncpy(name, path + raw_len - name_len, name_len + 1);
  if (i == 0) { // preserve "/" special case
    path[i + 1] = '\0';
  } else {
    path[i] = '\0';
  }
}
