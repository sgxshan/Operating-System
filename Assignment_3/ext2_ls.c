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

unsigned char *disk;


int main(int argc, char **argv) {

  if (argc != 3) {
    fprintf(stderr, "Usage: ext2_ls <image file name> <abs path>\n");
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

  disk = mmap(NULL, 128 * 1024, PROT_READ, MAP_SHARED, fd, 0);
  if (disk == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  struct ext2_group_desc* gd = (void*)disk + 1024 + EXT2_BLOCK_SIZE;
  void* inodes = disk + EXT2_BLOCK_SIZE * gd->bg_inode_table;

  int dir_inode_idx = find_dir_inode(query, inodes);
  if (dir_inode_idx < 0) {
    printf("No such file or directory\n");
    exit(1);
  }

  struct ext2_inode* inode = inodes + sizeof(struct ext2_inode) * dir_inode_idx;
  int total_size = inode->i_size;

  int block_num_idx = 0;
  int curr_size = 0;
  char* fname;
  int block_num = inode->i_block[block_num_idx];
  struct ext2_dir_entry_2* dir = (struct ext2_dir_entry_2*)
    (disk + EXT2_BLOCK_SIZE * block_num);
  while (curr_size < total_size) {
    curr_size += dir->rec_len;
    int i;
    fname = dir->name;
    for (i = 0; i < dir->name_len; i++, fname++) {
      printf("%c", *fname);
    }
    if (dir->rec_len != EXT2_BLOCK_SIZE) {
      // empty block -> do not print \n
      printf("\n");
    }
    dir = (void*)dir + dir->rec_len;
    if (curr_size % EXT2_BLOCK_SIZE == 0) { // need to use next pointer
      block_num_idx++;
      block_num = inode->i_block[block_num_idx];
      dir = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
    }
  }

  return 0;
}
