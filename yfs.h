#include <stdbool.h>
#include <comp421/iolib.h>
#include "iolib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <comp421/filesystem.h>

typedef struct queue queue;
typedef struct ht_map ht_map;
typedef struct ht ht;
typedef struct free_inode free_inode;
typedef struct free_block free_block;
typedef struct cache_item cache_item;


void make_free_lists();

void add_free_i_list(int inodeNum);
int get_next_free_bnum();
int get_dir_entry(char *pathname, int inodeStartNumber, int *blockNumPtr, bool createIfNeeded);
int get_nth_b(struct inode *inode, int n, bool allocateIfNeeded);
void add_b_freelist(int blockNum);
void evict_b();
void evict_i();
int get_path_i_num(char *path, int inodeStartNumber);
void handle_i(int inodeNum, int blockNum, struct fs_header *header, bool *takenBlocks);
void handle_b(int i, struct fs_header *header, bool *takenBlocks);
void *get_b(int b_num);
struct inode* get_i(int inodeNum);

struct ht *init_ht(double load_factor, int size);
int insert_ht(struct ht *ht, int key, void *value);
void *query_ht(struct ht *ht, int key);
void evict_ht(struct ht *ht, int key);
unsigned int value_ht(int key, struct ht *ht);

bool is_valid_path(char *pathname);

int get_create_file(char *filename, int dirInodeNum, int inodeNumToSet);
void zero_file(int inodeNum);
int make_file(char *filename, struct dir_entry *dir_entry, int inodeNumToSet);

void read_update(void **buf, int *blockOffset, int *bytesLeft, int *bytesToCopy);
int get_num_bytes_left(int byteOffset, int size, struct inode *inode);
int is_valid_inputs(int inodeNum, void *buf, int size, int byteOffset);

int is_valid_i(struct inode *inode);
int get_num_copy_bytes(int bytesLeft, int blockOffset);
void write_update(void **buf, int *blockOffset, int *bytesLeft, int *bytesToCopy);
void update_i_size(struct inode *inode, int size, int byteOffset, int bytesLeft);

bool is_valid_path_i(char *pathname, int curr_i);
int change_path_i(char *pathname, int curr_i);
void set_dir_entry(struct dir_entry *dir_entry, char *filename);
void set_newdir_i(int inodeNum, int dirInodeNum);
void clear_file(struct inode *inode, int inodeNum);

int handle_create(char *pathname, int curr_i, int inodeNumToSet);
int handle_open(char *pathname, int curr_i);
int handle_read(int inodeNum, void *buf, int size, int byteOffset, int pid);
int handle_write(int inodeNum, void *buf, int size, int byteOffset, int pid);
int handle_link(char *old_name, char *new_name, int curr_i);
int handle_unlink(char *pathname, int curr_i);
int handle_mkdir(char *pathname, int curr_i);
int handle_rmdir(char *pathname, int curr_i);
int handle_chdir(char *pathname, int curr_i);
int handle_stat(char *pathname, int curr_i, struct Stat *stat_buffer, int pid);
int handle_sync(void);
int handle_shutdown(void);
int handle_seek(int inodeNum, int offset, int whence, int currentPosition);

char * get_proc_path(int pid, char *pathname, int len);

cache_item *left_dequeue(queue *queue);
void right_dequeue(queue *queue, cache_item *item);
void right_enqueue(cache_item *item, queue *queue);
char *get_next_path_part(char *path);
void add_free_i_list(int inodeNum);
int get_super_dir(char *pathname, int curr_i, char **filenamePtr);
int get_next_inum_free();