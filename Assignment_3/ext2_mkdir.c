#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "util.h"

#define align4(x) ((x - 1) / 4 + 1) * 4

unsigned char *disk;


int main(int argc, char **argv) {

  if (argc != 3) {
    fprintf(stderr, "Usage: ext2_mkdir <image file name> <target path>\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDWR);

  int query_len = strlen(argv[2]); 
  char* query = malloc(query_len + 2);
  strcpy(query, argv[2]);
  if (query[0] != '/') {
    fprintf(stderr, "please use abs path!\n");
    exit(1);
  }
  if (strcmp(query, "/") != 0 && query[query_len - 1] == '/') {
    // strip trailing '/' for dir not root
    query[query_len - 1] = '\0';
  }

  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (disk == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  struct ext2_group_desc* gd = (void*)disk + 1024 + EXT2_BLOCK_SIZE;
  void* inodes = disk + EXT2_BLOCK_SIZE * gd->bg_inode_table;
  void* inode_bm_loc = disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap;
  void* block_bm_loc = disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap;
  int* inode_bm = get_inode_bitmap(inode_bm_loc);
  int* block_bm = get_block_bitmap(block_bm_loc);
  
  // check if dir already exist
  int already_inode_idx = find_dir_inode(query, inodes);
  if (already_inode_idx != -1) {
    // dir already exist
    errno = EEXIST;
    perror(NULL);
    exit(errno);
  }    

  // separate parent path and new dir name
  int i;
  char new_dir[512];
  separate(query, new_dir);
  int name_len = strlen(new_dir);

  // find parent inode
  int p_inode_idx = find_dir_inode(query, inodes);
  if (p_inode_idx < 0) {
    // parent dir not found
    errno = ENOENT;
    perror(query);
    exit(errno);
  }

  // check if new dir name is already a file under parent dir
  struct ext2_inode* p_inode =
    inodes + sizeof(struct ext2_inode) * p_inode_idx;
  int dir_num_blocks = p_inode->i_size / EXT2_BLOCK_SIZE;
  int block_num;
  struct ext2_dir_entry_2* dir;
  for (i = 0; i < dir_num_blocks; i++) {
    block_num = p_inode->i_block[i];
    dir = (void*)disk + EXT2_BLOCK_SIZE * block_num;
    int curr_size = 0;
    while (curr_size < EXT2_BLOCK_SIZE) {
      curr_size += dir->rec_len;
      if (dir->file_type == EXT2_FT_REG_FILE &&
          name_len == dir->name_len &&
          strncmp(new_dir, dir->name, name_len) == 0) {
        // found new dir name as reg file
        errno = EEXIST;
        perror(new_dir);
        exit(errno);
      }
      dir = (void*)dir + dir->rec_len;
    }
  }

  // grab an inode for the new dir
  int inode_idx = -1;
  for (i = 0; i < INODES_COUNT; i++) {
    if (inode_bm[i] == 0) {
      inode_idx = i;
      break;
    }
  }
  if (inode_idx == -1) {
    errno = ENOSPC;
    perror("no free inode");
    exit(errno);
  }

  // grab a free block for new dir to rest in
  int* free_blocks = find_free_blocks(block_bm, 1);
  if (free_blocks == NULL) {
    errno = ENOSPC;
    perror("not enough free blocks for file content");
    exit(errno);
  }
  int block_idx = *free_blocks;

  // default dir entry for new dir
  struct ext2_dir_entry_2* c_dir =
    (void*)disk + EXT2_BLOCK_SIZE * (block_idx + 1);
  // .
  c_dir->inode = inode_idx + 1;
  c_dir->rec_len = 12;
  c_dir->name_len = 1;
  c_dir->file_type = EXT2_FT_DIR;
  c_dir->name[0] = '.';
  c_dir = (void*)c_dir + 12;
  // ..
  c_dir->inode = p_inode_idx + 1;
  c_dir->rec_len = 1012;
  c_dir->name_len = 2;
  c_dir->file_type = EXT2_FT_DIR;
  c_dir->name[0] = '.';
  c_dir->name[1] = '.';
  // udpate block bitmap
  set_block_bitmap(block_bm_loc, block_idx, 1);

  // new inode for new dir
  struct ext2_inode* inode = inodes + sizeof(struct ext2_inode) * inode_idx;
  inode->i_mode = EXT2_S_IFDIR;
  inode->i_size = EXT2_BLOCK_SIZE;
  inode->i_links_count = 2;
  inode->i_blocks = 2;
  inode->i_block[0] = block_idx + 1;
  // update inode bitmap
  set_inode_bitmap(inode_bm_loc, inode_idx, 1);

  // update links count for parent dir
  p_inode->i_links_count++;

  // put a new dir_ent for new dir in parent dir block
  int size_required = align4(name_len) + 8;
  bool found = false;
  for (i = 0; i < dir_num_blocks; i++) {
    block_num = p_inode->i_block[i];
    dir = (void*)disk + EXT2_BLOCK_SIZE * block_num;
    int curr_size = 0;
    while (curr_size < EXT2_BLOCK_SIZE) {
      curr_size += dir->rec_len;
      if (curr_size == EXT2_BLOCK_SIZE &&
          dir->rec_len >= size_required + 8 + align4(dir->name_len)) {
        found = true;
        break;
      }
      dir = (void*)dir + dir->rec_len;
    }
    if (found) {
      break;
    }
  }
  if (!found) {
    // reserve one more for potential extending a block for dir
    block_bm = get_block_bitmap(block_bm_loc);
    int* free_block = find_free_blocks(block_bm, 1);
    if (free_block == NULL) {
      errno = ENOSPC;
      perror("no free block for new directory entry");
      exit(errno);
    }
    int new_block_idx = *free_block;
    dir = (void*)disk + EXT2_BLOCK_SIZE * (new_block_idx + 1);
    dir->inode = inode_idx + 1;
    dir->rec_len = EXT2_BLOCK_SIZE;
    dir->name_len = name_len;
    dir->file_type = EXT2_FT_DIR;
    strncpy((void*)dir + 8, new_dir, name_len);
    // update block bitmap
    set_block_bitmap(block_bm_loc, new_block_idx, 1);
    // update parent dir inode
    p_inode->i_size += EXT2_BLOCK_SIZE;
    p_inode->i_blocks += 2;
    p_inode->i_block[dir_num_blocks] = new_block_idx + 1;
  } else {
    int prev_rec_len = dir->rec_len;
    int adj_len = align4(dir->name_len) + 8;
    dir->rec_len = adj_len;
    dir = (void*)dir + adj_len;
    dir->inode = inode_idx + 1;
    dir->rec_len = prev_rec_len - adj_len;
    dir->name_len = name_len;
    dir->file_type = EXT2_FT_DIR;
    strncpy((void*)dir + 8, new_dir, name_len);
  }

  // update group desc table
  // possible changes: blocks, inodes, used_dir
  gd->bg_free_blocks_count = total_free_blocks(block_bm_loc);
  gd->bg_free_inodes_count = total_free_inodes(inode_bm_loc);
  gd->bg_used_dirs_count++;
  
  return 0;
}
