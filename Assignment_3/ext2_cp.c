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
            "Usage: ext2_cp <image file name> <source file> <target path>\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDWR);
  int src_fd = open(argv[2], O_RDONLY);

  if (src_fd == -1) {
    // source file not found
    errno = ENOENT;
    perror(argv[2]);
    exit(errno);
  }

  int fsize = lseek(src_fd, 0, SEEK_END);
  int num_blocks = (fsize - 1) / EXT2_BLOCK_SIZE + 1;

  int query_len = strlen(argv[3]); 
  char* query = malloc(query_len + 2);
  strcpy(query, argv[3]);
  if (query[0] != '/') {
    fprintf(stderr, "please use abs path!\n");
    exit(1);
  }
  bool force_dir = false; // mark /123/ /123 different
  if (strcmp(query, "/") != 0 && query[query_len - 1] == '/') {
    // strip trailing '/' for dir not root
    query[query_len - 1] = '\0';
  }

  // extract only filename from src file path
  int i;
  char name_new_file[512];
  int raw_len = strlen(argv[2]);
  i = raw_len - 1;
  while (i >= 0 && argv[2][i] != '/') {
    i--;
  }
  int name_len = raw_len - i - 1;
  strncpy(name_new_file, argv[2] + raw_len - name_len, name_len + 1);
  
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

  // find parent directory
  int p_inode_idx = find_dir_inode(query, inodes);
  if (p_inode_idx < 0) {
    // query can only mean dir
    if (force_dir) {
      errno = ENOENT;
      perror("illegal target path");
      exit(errno);
    }
    // possible des: /level1/newfile not a dir
    // new parent dir: /level1
    // new file name: newfile
    separate(query, name_new_file);
    name_len = strlen(name_new_file);
    p_inode_idx = find_dir_inode(query, inodes);
    if (p_inode_idx < 0) {
      errno = ENOENT;
      perror("illegal target path");
      exit(errno);
    }
  }

  // grab an inode for the new file
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

  // if target file already exist, throw error
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
          strncmp(name_new_file, dir->name, name_len) == 0) {
        // found reg file with same name
        errno = EEXIST;
        perror(name_new_file);
        exit(errno);
      }
      dir = (void*)dir + dir->rec_len;
    }
  }

  // grab correct number of blocks for the new file
  bool indirect = false;
  if (num_blocks > 12) {
    num_blocks++; // one more master block on single indrect
    indirect = true;
  }
  int* free_blocks = find_free_blocks(block_bm, num_blocks);
  if (free_blocks == NULL) {
    errno = ENOSPC;
    perror("not enough free blocks for file content");
    exit(errno);
  }

  unsigned char* src = mmap(NULL, fsize, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE, src_fd, 0);

  // copy file content block by block
  int already_copied = 0;
  int fsize_remain = fsize;
  int num_write_blocks = indirect ? num_blocks - 1 : num_blocks;
  for (i = 0; i < num_write_blocks; i++) {
    int block_idx = free_blocks[i];
    // update block bitmap
    set_block_bitmap(block_bm_loc, block_idx, 1);
    if (fsize_remain < EXT2_BLOCK_SIZE) {
      memcpy(disk + EXT2_BLOCK_SIZE * (block_idx + 1),
             src + already_copied, fsize_remain);
      fsize_remain = 0;
      break;
    } else {
      memcpy(disk + EXT2_BLOCK_SIZE * (block_idx + 1),
             src + already_copied, EXT2_BLOCK_SIZE);
      fsize_remain -= EXT2_BLOCK_SIZE;
      already_copied += EXT2_BLOCK_SIZE;
    }
  }
  int master_block_idx = -1;
  // deal with single indirect
  if (indirect) {
    master_block_idx = free_blocks[num_blocks - 1];
    set_block_bitmap(block_bm_loc, master_block_idx, 1);
    int* data = (void*)disk + EXT2_BLOCK_SIZE * (master_block_idx + 1);
    for (i = 12; i < num_blocks - 1; i++) {
      *data = free_blocks[i] + 1;
      data++;
    }
  }

  // new inode for copied file
  struct ext2_inode* inode = inodes + sizeof(struct ext2_inode) * inode_idx;
  inode->i_mode = EXT2_S_IFREG;
  inode->i_size = fsize;
  inode->i_links_count = 1;
  inode->i_blocks = num_blocks * 2;
  for (i = 0; i < num_blocks; i++) {
    if (i > 11) {
      break;
    }
    inode->i_block[i] = free_blocks[i] + 1;
  }
  if (indirect) {
    inode->i_block[12] = master_block_idx + 1;
  }
  // update inode bitmap
  set_inode_bitmap(inode_bm_loc, inode_idx, 1);

  // put a new dir_ent for new file in parent dir block
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
    dir->file_type = EXT2_FT_REG_FILE;
    strncpy((void*)dir + 8, name_new_file, name_len);
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
    dir->file_type = EXT2_FT_REG_FILE;
    strncpy((void*)dir + 8, name_new_file, name_len);
  }

  // update group desc table
  // possible changes: blocks, inodes
  gd->bg_free_blocks_count = total_free_blocks(block_bm_loc);
  gd->bg_free_inodes_count = total_free_inodes(inode_bm_loc);

  return 0;
}
