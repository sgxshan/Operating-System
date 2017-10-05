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
    fprintf(stderr, 
            "Usage: ext2_rm <image file name> <file path>\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDWR);
  char* path = argv[2];
  if (path[0] != '/') {
    fprintf(stderr, "please use abs path!\n");
    exit(1);
  }

  int raw_len = strlen(path);
  if (path[raw_len - 1] == '/') {
    // only take file path, not dir path
    errno = EISDIR;
    perror(path);
    exit(errno);
  }

  int i;
  // separate parent dir and file name
  char file_name[512];
  separate(path, file_name);
  int name_len = strlen(file_name);
  
  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (disk == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  struct ext2_group_desc* gd = (void*)disk + 1024 + EXT2_BLOCK_SIZE;
  void* inodes = disk + EXT2_BLOCK_SIZE * gd->bg_inode_table;
  void* inode_bm_loc = disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap;
  void* block_bm_loc = disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap;

  // find parent dir
  int dir_inode_idx = find_dir_inode(path, inodes);
  if (dir_inode_idx < 0) {
    // parent dir not found
    errno = ENOENT;
    perror(path);
    exit(errno);
  }

  // find target file under parent dir
  int file_inode_idx = -1;
  struct ext2_inode* dir_inode =
    inodes + sizeof(struct ext2_inode) * dir_inode_idx;
  int dir_num_blocks = dir_inode->i_size / EXT2_BLOCK_SIZE;
  int block_num;
  struct ext2_dir_entry_2* dir;
  for (i = 0; i < dir_num_blocks; i++) {
    block_num = dir_inode->i_block[i];
    dir = (void*)disk + EXT2_BLOCK_SIZE * block_num;
    int curr_size = 0;
    int last_rec_len = 0;
    int curr_pos = 0;
    while (curr_size < EXT2_BLOCK_SIZE) {
      curr_size += dir->rec_len;
      if (dir->file_type == EXT2_FT_REG_FILE &&
          name_len == dir->name_len &&
          strncmp(file_name, dir->name, name_len) == 0) {
        file_inode_idx = dir->inode - 1;
        break;
      }
      last_rec_len = dir->rec_len;
      dir = (void*)dir + dir->rec_len;
      curr_pos += dir->rec_len;
    }
    if (file_inode_idx != -1) {
      if (dir->rec_len == EXT2_BLOCK_SIZE) {
        // if dir block can be recycled
        // when file is the only one in block
        dir_inode->i_blocks -= 2;
        dir_inode->i_size -= EXT2_BLOCK_SIZE;
        set_block_bitmap(block_bm_loc, block_num - 1, 0);
      } else {
        // remove dir entry record
        if (curr_pos == 0) {
          // dir entry is the first dir entry in the block
          // and there are dir entries that follows
          int rec_len = dir->rec_len;
          void* src_start = (void*)dir + rec_len;
          void* des_start = (void*)dir;
          // find last dir entry
          // add rec_len to that
          // move all the data up
          while (curr_size < EXT2_BLOCK_SIZE) {
            curr_size += dir->rec_len;
            last_rec_len = dir->rec_len;
            dir = (void*)dir + dir->rec_len;
          }
          dir = (void*)dir - last_rec_len;
          dir->rec_len += rec_len;
          memcpy(des_start, src_start, EXT2_BLOCK_SIZE - rec_len);
        } else {
          // normal case
          // let last dir entry have more rec_len
          int rec_len = dir->rec_len;
          dir = (void*)dir - last_rec_len;
          dir->rec_len = rec_len + last_rec_len;
        }
      }
      break;
    }
  }
  if (file_inode_idx == -1) {
    // target file not found
    errno = ENOENT;
    perror(file_name);
    exit(errno);
  }

  // decrement inode link count
  struct ext2_inode* file_inode =
    inodes + sizeof(struct ext2_inode) * file_inode_idx;
  file_inode->i_links_count--;
  // release inodes and blocks
  if (file_inode->i_links_count == 0) {
    int num_blocks = file_inode->i_blocks / 2;
    if (num_blocks > 12) {
      for (i = 0; i < 12; i++) {
        set_block_bitmap(block_bm_loc, file_inode->i_block[i] - 1, 0);
      }
      int master_block_idx = file_inode->i_block[12] - 1;
      set_block_bitmap(block_bm_loc, master_block_idx, 0);
      int* blocks = (void*)disk + EXT2_BLOCK_SIZE * (master_block_idx + 1);
      for (i = 0; i < num_blocks - 13; i++) {
        set_block_bitmap(block_bm_loc, *blocks - 1, 0);
        blocks++;
      }
    } else {
      for (i = 0; i < num_blocks; i++) {
        set_block_bitmap(block_bm_loc, file_inode->i_block[i] - 1, 0);
      }
    }
    set_inode_bitmap(inode_bm_loc, file_inode_idx, 0);
  }

  // update group desc table
  // possible changes: blocks, inodes
  gd->bg_free_blocks_count = total_free_blocks(block_bm_loc);
  gd->bg_free_inodes_count = total_free_inodes(inode_bm_loc);
  
  return 0;
}
