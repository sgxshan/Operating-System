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

  if (argc != 4) {
    fprintf(stderr, 
            "Usage: ext2_ln <image file name> <source path> <target path>\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDWR);
  char* src = argv[2];
  char* des = argv[3];
  if (src[0] != '/' || des[0] != '/') {
    fprintf(stderr, "please use abs path!\n");
    exit(1);
  }

  int raw_src_len = strlen(src);
  int raw_des_len = strlen(des);
  if (src[raw_src_len - 1] == '/') {
    errno = EISDIR;
    perror(src);
    exit(errno);
  }
  if (des[raw_des_len - 1] == '/') {
    errno = EISDIR;
    perror(des);
    exit(errno);
  }

  if (strcmp(src, "/") != 0 && src[raw_src_len - 1] == '/') {
    // strip trailing '/' for dir not root
    src[raw_src_len - 1] = '\0';
  }
  if (strcmp(des, "/") != 0 && des[raw_des_len - 1] == '/') {
    // strip trailing '/' for dir not root
    des[raw_des_len - 1] = '\0';
  }

  int i;
  // separate filenames with parent dir
  char src_name[512];
  separate(src, src_name);
  int src_name_len = strlen(src_name);
  char des_name[512];
  separate(des, des_name);
  int des_name_len = strlen(des_name);

  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(disk == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  struct ext2_group_desc* gd = (void*)disk + 1024 + EXT2_BLOCK_SIZE;
  void* inodes = disk + EXT2_BLOCK_SIZE * gd->bg_inode_table;
  void* block_bm_loc = disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap;
  int* block_bm = get_block_bitmap(block_bm_loc);

  // find src dir, des dir
  int src_inode_idx = find_dir_inode(src, inodes);
  int des_inode_idx = find_dir_inode(des, inodes);
  if (src_inode_idx < 0) {
    errno = ENOENT;
    perror(src);
    exit(errno);
  }
  if (des_inode_idx < 0) {
    errno = ENOENT;
    perror(des);
    exit(errno);
  }

  // find src file under src dir
  int src_file_inode_idx = -1;
  struct ext2_inode* src_inode =
    inodes + sizeof(struct ext2_inode) * src_inode_idx;
  int dir_num_blocks = src_inode->i_size / EXT2_BLOCK_SIZE;
  int block_num;
  struct ext2_dir_entry_2* dir;
  for (i = 0; i < dir_num_blocks; i++) {
    block_num = src_inode->i_block[i];
    dir = (void*)disk + EXT2_BLOCK_SIZE * block_num;
    int curr_size = 0;
    while (curr_size < EXT2_BLOCK_SIZE) {
      curr_size += dir->rec_len;
      if (dir->file_type == EXT2_FT_REG_FILE &&
          src_name_len == dir->name_len &&
          strncmp(src_name, dir->name, src_name_len) == 0) {
        src_file_inode_idx = dir->inode - 1;
        break;
      }
      dir = (void*)dir + dir->rec_len;
    }
    if (src_file_inode_idx != -1) {
      break;
    }
  }
  if (src_file_inode_idx == -1) {
    // src file not found in src dir
    errno = ENOENT;
    perror(src_name);
    exit(errno);
  }

  // find if des file already exist as file or dir
  int des_file_inode_idx = -1;
  struct ext2_inode* des_inode =
    inodes + sizeof(struct ext2_inode) * des_inode_idx;
  dir_num_blocks = des_inode->i_size / EXT2_BLOCK_SIZE;
  for (i = 0; i < dir_num_blocks; i++) {
    block_num = des_inode->i_block[i];
    dir = (void*)disk + EXT2_BLOCK_SIZE * block_num;
    int curr_size = 0;
    while (curr_size < EXT2_BLOCK_SIZE) {
      curr_size += dir->rec_len;
      if (des_name_len == dir->name_len &&
          strncmp(des_name, dir->name, des_name_len) == 0) {
        des_file_inode_idx = dir->inode - 1;
        break;
      }
      dir = (void*)dir + dir->rec_len;
    }
    if (des_file_inode_idx != -1) {
      break;
    }
  }
  if (des_file_inode_idx != -1) {
    // des file found in des dir
    errno = EEXIST;
    perror(des_name);
    exit(errno);
  }

  // put a new dir_ent for new file in parent dir block
  int size_required = align4(des_name_len) + 8;
  bool found = false;
  for (i = 0; i < dir_num_blocks; i++) {
    block_num = des_inode->i_block[i];
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
    dir->inode = src_file_inode_idx + 1;
    dir->rec_len = EXT2_BLOCK_SIZE;
    dir->name_len = des_name_len;
    dir->file_type = EXT2_FT_REG_FILE;
    strncpy((void*)dir + 8, des_name, des_name_len);
    // update block bitmap
    set_block_bitmap(block_bm_loc, new_block_idx, 1);
    // update parent dir inode
    des_inode->i_size += EXT2_BLOCK_SIZE;
    des_inode->i_blocks += 2;
    des_inode->i_block[dir_num_blocks] = new_block_idx + 1;
  } else {
    int prev_rec_len = dir->rec_len;
    int adj_len = align4(dir->name_len) + 8;
    dir->rec_len = adj_len;
    dir = (void*)dir + adj_len;
    dir->inode = src_file_inode_idx + 1;
    dir->rec_len = prev_rec_len - adj_len;
    dir->name_len = des_name_len;
    dir->file_type = EXT2_FT_REG_FILE;
    strncpy((void*)dir + 8, des_name, des_name_len);
  }

  // update src file inode links count
  struct ext2_inode* inode =
    inodes + sizeof(struct ext2_inode) * src_file_inode_idx;
  inode->i_links_count++;

  return 0;
}
