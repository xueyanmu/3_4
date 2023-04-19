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

void add_free_i_list(int i_num);
int get_next_free_bnum();
int get_dir_entry(char *pathname, int inodeStartNumber, int *b_numPtr, bool createIfNeeded);
int get_nth_b(struct inode *inode, int n, bool allocateIfNeeded);
void add_b_freelist(int b_num);
void evict_b();
void evict_i();
int get_path_i_num(char *path, int inodeStartNumber);
void handle_i(int i_num, int b_num, struct fs_header *header, bool *takenBlocks);
void handle_b(int i, struct fs_header *header, bool *takenBlocks);
void *get_b(int b_num);
struct inode* get_i(int i_num);

struct ht *init_ht(double load_factor, int size);
int insert_ht(struct ht *ht, int key, void *value);
void *query_ht(struct ht *ht, int key);
void evict_ht(struct ht *ht, int key);
unsigned int value_ht(int key, struct ht *ht);

bool is_valid_path(char *pathname);

int get_create_file(char *filename, int dir_i_num, int new_i_num);
void zero_file(int i_num);
int make_file(char *filename, struct dir_entry *dir_entry, int new_i_num);

void read_update(void **buf, int *b_offset, int *leftover_bytes, int *num_bytes);
int get_num_bytes_left(int byte_offset, int size, struct inode *inode);
int is_valid_inputs(int i_num, void *buf, int size, int byte_offset);

int is_valid_i(struct inode *inode);
int get_num_copy_bytes(int leftover_bytes, int b_offset);
void write_update(void **buf, int *b_offset, int *leftover_bytes, int *num_bytes);
void update_i_size(struct inode *inode, int size, int byte_offset, int leftover_bytes);

bool is_valid_path_i(char *pathname, int curr_i);
int change_path_i(char *pathname, int curr_i);
void set_dir_entry(struct dir_entry *dir_entry, char *filename);
void set_newdir_i(int i_num, int dir_i_num);
void clear_file(struct inode *inode, int i_num);

int handle_create(char *pathname, int curr_i, int new_i_num);
int handle_open(char *pathname, int curr_i);
int handle_read(int i_num, void *buf, int size, int byte_offset, int pid);
int handle_write(int i_num, void *buf, int size, int byte_offset, int pid);
int handle_link(char *old_name, char *new_name, int curr_i);
int handle_unlink(char *pathname, int curr_i);
int handle_mkdir(char *pathname, int curr_i);
int handle_rmdir(char *pathname, int curr_i);
int handle_chdir(char *pathname, int curr_i);
int handle_stat(char *pathname, int curr_i, struct Stat *stat_buffer, int pid);
int handle_sync(void);
int handle_shutdown(void);
int handle_seek(int i_num, int offset, int whence, int currentPosition);

char * get_proc_path(int pid, char *pathname, int len);

cache_item *left_dequeue(queue *queue);
void right_dequeue(queue *queue, cache_item *item);
void right_enqueue(cache_item *item, queue *queue);
char *get_next_path_part(char *path);
void add_free_i_list(int i_num);
int get_super_dir(char *pathname, int curr_i, char **filenamePtr);
int get_next_inum_free();