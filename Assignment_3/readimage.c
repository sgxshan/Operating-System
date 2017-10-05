#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <string.h>
#include "util.h"

unsigned char *disk;


int main(int argc, char **argv) {

  if(argc != 2) {
    fprintf(stderr, "Usage: readimg <image file name>\n");
    exit(1);
  }
  int fd = open(argv[1], O_RDWR);

  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(disk == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  printf("Inodes: %d\n", sb->s_inodes_count);
  printf("Blocks: %d\n", sb->s_blocks_count);
    
  // ex17
  struct ext2_group_desc* gd = (struct ext2_group_desc*)
    (disk + 1024 + EXT2_BLOCK_SIZE);
  printf("Block group:\n");
  printf("    block bitmap: %d\n", gd->bg_block_bitmap);
  printf("    inode bitmap: %d\n", gd->bg_inode_bitmap);
  printf("    inode table: %d\n", gd->bg_inode_table);
  printf("    free blocks: %d\n", gd->bg_free_blocks_count);
  printf("    free inodes: %d\n", gd->bg_free_inodes_count);
  printf("    used_dirs: %d\n", gd->bg_used_dirs_count);

  // ex18
  unsigned char buf;
  char* ptr;
  int i, j, off;

  // block bitmap
  char* block_bm = (char*)(disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
  ptr = block_bm;
  printf("Block bitmap: ");
  for (i = 0; i < sb->s_blocks_count / 8; i++, ptr++) {
    buf = *ptr; // 8 bits are read backwards
    for (off = 0; off < 8; off++) {
      printf("%d", (buf >> off) & 0x1);
    }
    printf(" ");
  }
  printf("\n");

  // inode bitmap
  char* inode_bm = (char*)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
  int* bm = get_inode_bitmap(inode_bm);
  ptr = inode_bm;
  printf("Inode bitmap: ");
  for (i = 0; i < sb->s_inodes_count / 8; i++, ptr++) {
    buf = *ptr; // 8 bits are read backwards
    for (off = 0; off < 8; off++) {
      printf("%d", (buf >> off) & 0x1);
    }
    printf(" ");
  }
  printf("\n");

  // inodes details
  char* inodes = (char*)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
  char type = 'U'; // unknown, default value
  struct ext2_inode* inode;

  // dir bitmap, size
  int* dir_bm = malloc(sizeof(int) * sb->s_inodes_count);
  int* dir_size = malloc(sizeof(int) * sb->s_inodes_count);
  memset(dir_bm, 0, sizeof(int) * sb->s_inodes_count);
  memset(dir_size, 0, sizeof(int) * sb->s_inodes_count);
                 
    
  // inode 2 root directory
  // other inodes < 12 don't care, reserved
  printf("\nInodes:\n");
  for (i = EXT2_ROOT_INO - 1; i < sb->s_inodes_count; i++) {
    if (i < EXT2_GOOD_OLD_FIRST_INO && i != EXT2_ROOT_INO - 1) {
      continue;
    }
    inode = (struct ext2_inode*)(inodes + sizeof(struct ext2_inode) * i);
    if (bm[i] == 0) { // means no data?       
      continue;
    }
    if (inode->i_mode & EXT2_S_IFREG) {
      type = 'f';
    } else if (inode->i_mode & EXT2_S_IFDIR) {
      type = 'd';
      dir_bm[i] = 1;
      dir_size[i] = inode->i_size;
    } // no else at the moment
    printf("[%d] type: %c size: %d links: %d blocks: %d\n",
           i + 1, type, inode->i_size, inode->i_links_count, inode->i_blocks);
    printf("[%d] Blocks: ", i + 1);
    int num_blocks = inode->i_blocks / 2;
    for (j = 0; j < num_blocks; j++) {
      if (j > 11) {
        break;
      }
      printf(" %d", inode->i_block[j]);
    }
    if (num_blocks > 12) {
      num_blocks -= 13;
      int indirect_block = inode->i_block[12];
      int* data = (void*)disk + EXT2_BLOCK_SIZE * indirect_block;
      printf(" [%d] (", indirect_block);
      while (num_blocks-- > 0) {
        printf(" %d", *data);
        data++;
      }
      printf(" )");
    }
    printf("\n");
  }

  // ex19
  printf("\nDirectory Blocks:\n");
  for (i = EXT2_ROOT_INO - 1; i < sb->s_inodes_count; i++) {
    if (dir_bm[i] == 0) {
      continue;
    }
    inode = (struct ext2_inode*)(inodes + sizeof(struct ext2_inode) * i);
    unsigned int* block_num_ptr = inode->i_block;
    unsigned int block_num;
    int num_blocks = inode->i_blocks / 2;
    int k;
    for (k = 0; k < num_blocks; k++) {
      block_num = inode->i_block[k];
      printf("   DIR BLOCK NUM: %d ", block_num);
      struct ext2_dir_entry_2* dir;
      dir = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
      printf("(for inode %d)\n", i + 1);
      int curr_size = 0;
      while (curr_size < EXT2_BLOCK_SIZE) {
        curr_size += dir->rec_len;
        printf("Inode: %d rec_len: %d ", dir->inode, dir->rec_len);
        printf("name_len: %d ", dir->name_len);
        char* fname = dir->name;
        switch (dir->file_type) {
        case EXT2_FT_REG_FILE:
          printf("type= f ");
          break;
        case EXT2_FT_DIR:
          printf("type= d ");
          break;
        }
        printf("name=");
        for (j = 0; j < dir->name_len; j++, fname++) {
          printf("%c", *fname);
        }
        printf("\n");
        dir = (void*)dir + dir->rec_len;
      }
      block_num_ptr++;
    }
  }

  return 0;
}
