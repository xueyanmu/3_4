#include "yfs.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <comp421/hardware.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include <stdlib.h>
#include <string.h>

struct ht_map {
	int key;
	void *value;
	ht_map *next_inode; 
};

struct ht {
	ht_map **head;
	unsigned int size;
	unsigned int num_used;		
	double load_factor;
};

struct cache_item {
    int c_item_num;
    bool dirty;
    void *addr;
    cache_item *prev_c_item;
    cache_item *next_c_item;
};

struct free_inode {
    int i_num;
    free_inode *next_inode;
};

struct free_block {
    int b_num;
    free_block *next_inode;
};

struct queue {
    cache_item *q_first;
    cache_item *q_last;
};


free_inode *i_head = NULL;
free_block *b_head = NULL;

int free_i_count = 0;
int free_b_count = 0;
int curr_i = ROOTINODE;

queue *cache_i_queue;
queue *cache_b_queue;

struct ht *i_ht;
struct ht *b_ht;

int cache_i_size = 0;
int cache_b_size = 0;

/**
*
 * We provide a Unix (rather than a Yalnix) program to format an empty, validly-formatted Yalnix file
system on the disk. To use this program, execute the Unix program
/clear/courses/comp421/pub/bin/mkyfs

(Run this program just as shown above from a Unix shell; do not run it under Yalnix.) This will create a
YFS file system with 47 inodes (6 blocks worth of inodes). If you want a different c_item_num of inodes, put
the c_item_num of inodes as the command line argument. The file system will contain only a root directory with
“.” and “..” in it. Run mkyfs as a Unix command, not under Yalnix, from the same directory where you
will run Yalnix.
If you want to, you can modify this mkyfs.c program to set up test cases for yourself; the source code
to this program is in the file /clear/courses/comp421/pub/samples-lab3/mkyfs.c. For
example, before you get the MkDir file system request working correctly in your server, you can modify
mkyfs.c to make test directories for yourself. Run that version of mkyfs before you then run your YFS
server under Yalnix, and you could then test looking up files in directories and subdirectories, even before
you get MkDir working in your server. This is just one example of what you could do to test some things
in your server before other things in your server are working correctly yet. Since mkyfs.c runs as a Unix
program, you can create anything in the Yalnix disk that you want to, and then run your server and see what
your server can do with the contents that it finds on the disk

FIRST RUN MKYFS ON UNIX/LINUX. This reads disk, building internal list of free blocks and inodes.
You can change the c_item_num of inodes if you want.


*/
void init()
{
    make_free_lists();

    if (Register(FILE_SERVER) != 0)
    {
        TracePrintf(1, "ERROR WITH REGISTERING FILE SERVER\n");
        Exit(1);
    };
}

int main(int argc, char **argv)
{
    init();

    if (argc > 1)
    {
        if (Fork() == 0)
        {
            Exec(argv[1], argv + 1);
        }
        else
        {
            while (true)
            {

                int ret;

                struct m_template tmp_received;

                int received_pid = Receive(&tmp_received);

                if (received_pid == ERROR)
                {
                    TracePrintf(1, "ERROR: MESSAGE NOT RECEIVED, SHUTTING DOWN YFS \n");
                    handle_shutdown();
                }

                // printf("Message received\n");
                int tmp = tmp_received.num;

                switch (tmp)
                {
                case YFS_OPEN:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_open(pathname, message->cur_i);
                    free(pathname);
                    break;
                }

                case YFS_CREATE:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_create(pathname, message->cur_i, -1);
                    free(pathname);
                    break;
                }

                case YFS_READ:
                {
                    struct m_file *message = (struct m_file *)&tmp_received;
                    ret = handle_read(message->i_num, message->buf, message->size, message->offset, received_pid);
                    break;
                }

                case YFS_WRITE:
                {
                    struct m_file *message = (struct m_file *)&tmp_received;
                    ret = handle_write(message->i_num, message->buf, message->size, message->offset, received_pid);
                    break;
                }

                case YFS_SEEK:
                {
                    struct m_seek *message = (struct m_seek *)&tmp_received;
                    ret = handle_seek(message->i_num, message->offset, message->whence, message->cur_pos);
                    break;
                }

                case YFS_LINK:
                {
                    struct m_link *message = (struct m_link *)&tmp_received;
                    char *old_name = get_proc_path(received_pid, message->old_name, message->old_len);
                    char *new_name = get_proc_path(received_pid, message->new_name, message->new_len);
                    ret = handle_link(old_name, new_name, message->cur_i);
                    free(old_name);
                    free(new_name);
                    break;
                }

                case YFS_UNLINK:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_unlink(pathname, message->cur_i);
                    free(pathname);
                    break;
                }

                case YFS_MKDIR:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_mkdir(pathname, message->cur_i);
                    free(pathname);
                    break;
                }

                case YFS_RMDIR:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_rmdir(pathname, message->cur_i);
                    free(pathname);
                    break;
                }

                case YFS_CHDIR:
                {
                    struct m_path *message = (struct m_path *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_chdir(pathname, message->cur_i);
                    free(pathname);
                    break;
                }

                case YFS_STAT:
                {
                    struct message_stat *message = (struct message_stat *)&tmp_received;
                    char *pathname = get_proc_path(received_pid, message->pathname, message->len);
                    ret = handle_stat(pathname, message->cur_i, message->stat_buffer, received_pid);
                    free(pathname);
                    break;
                }

                case YFS_SYNC:
                {
                    ret = handle_sync();
                    break;
                }

                case YFS_SHUTDOWN:
                {
                    ret = handle_shutdown();
                    break;
                }

                default:
                {
                    TracePrintf(1, "ERROR: INVALID INSTRUCTION %d\n", tmp_received.num);
                    ret = ERROR;
                    break;
                }

                }

                
                struct m_template msg_rply;
                msg_rply.num = ret;
                
                if (Reply(&msg_rply, received_pid) != 0)
                {
                    TracePrintf(1, "ERROR: REPLY TO PID: %d\n", received_pid);
                }
                // else {
                //     printf("Message replied\n");
                // }
            }
        }
    }

    return 0;
}

int handle_open(char *pathname, int curr_i)
{
    if (pathname == NULL || curr_i <= 0)
    {
        return ERROR;
    }
    if (pathname[0] == '/')
    {
        while (pathname[0] == '/')
            pathname += sizeof(char);
        curr_i = ROOTINODE;
    }
    int i_num = get_path_i_num(pathname, curr_i);
    if (i_num == 0)
    {
        return ERROR;
    }
    return i_num;
}

// Check if pathname is valid
bool isPathnameValid(char *pathname)
{
    if (pathname == NULL)
    {
        return false;
    }

    // Check if pathname length is within limits
    int i;
    for (i = 0; i < MAXPATHNAMELEN; i++)
    {
        if (pathname[i] == '\0')
        {
            break;
        }
    }
    if (i == MAXPATHNAMELEN)
    {
        return false;
    }

    // Check if pathname is valid
    i = 0;
    while ((pathname[i] != '\0') && (i < MAXPATHNAMELEN))
    {
        if (pathname[i] == '/')
        {
            if (i + 1 > MAXPATHNAMELEN || pathname[i + 1] == '\0')
            {
                return false;
            }
        }
        i++;
    }

    return true;
}

int handle_create(char *pathname, int curr_i, int new_i_num)
{
    if (!isPathnameValid(pathname) || curr_i <= 0)
    {
        return ERROR;
    }

    TracePrintf(1, "CREATE PATHNAME %s IN CURRENT INODE %d\n", pathname, curr_i);

    char *filename;
    int dir_i_num = get_super_dir(pathname, curr_i, &filename);
    if (dir_i_num == ERROR)
    {
        return ERROR;
    }

    struct inode *dir_i = get_i(dir_i_num);
    if (dir_i->type != INODE_DIRECTORY)
    {
        return ERROR;
    }

    int i_num = get_create_file(filename, dir_i_num, new_i_num);
    if (i_num == ERROR)
    {
        return ERROR;
    }

    return i_num;
}

int get_create_file(char *filename, int dir_i_num, int new_i_num)
{
    int b_num, offset;
    offset = get_dir_entry(filename, dir_i_num, &b_num, true);
    void *block = get_b(b_num);
    struct dir_entry *dir_entry = (struct dir_entry *)((char *)block + offset);
    int i_num = dir_entry->inum;
    if (i_num != 0)
    {
        if (new_i_num != -1)
        {
            return ERROR;
        }
        zero_file(i_num);
    }
    else
    {
        i_num = make_file(filename, dir_entry, new_i_num);
    }
    cache_item *b_tmp = (cache_item *)query_ht(b_ht, b_num);
    b_tmp->dirty = true;
    return i_num;
}

void zero_file(int i_num)
{
    struct inode *inode = get_i(i_num);
    int j = 0, b_num;
    while ((b_num = get_nth_b(inode, j++, false)) != 0)
    {
        free_block *new_b = malloc(sizeof(free_block));
        new_b->b_num = b_num;
        new_b->next_inode = b_head;
        b_head = new_b;
        free_b_count++;
    }
    inode->size = 0;
    cache_item *i_tmp = (cache_item *)query_ht(i_ht, i_num);
    i_tmp->dirty = true;
}

int make_file(char *filename, struct dir_entry *dir_entry, int new_i_num)
{
    strncpy(dir_entry->name, filename, DIRNAMELEN);
    //dont create new
    if (new_i_num == -1)
    {
        int i_num = get_next_inum_free();
        dir_entry->inum = i_num;
        struct inode *inode = get_i(i_num);
        inode->type = INODE_REGULAR;
        inode->size = 0;
        inode->nlink = 1;
        cache_item *i_tmp = (cache_item *)query_ht(i_ht, i_num);
        i_tmp->dirty = true;
        return i_num;
    }
    else
    {
        dir_entry->inum = new_i_num;
        return new_i_num;
    }
}

int is_valid_inputs(int i_num, void *buf, int size, int byte_offset)
{
    if (buf == NULL || size < 0 || byte_offset < 0 || i_num <= 0)
    {
        return false;
    }
    return true;
}

int get_num_bytes_left(int byte_offset, int size, struct inode *inode)
{
    int leftover_bytes = size;
    if (inode->size - byte_offset < size)
    {
        leftover_bytes = inode->size - byte_offset;
    }
    return leftover_bytes;
}

void read_update(void **buf, int *b_offset, int *leftover_bytes, int *num_bytes)
{
    *buf += *num_bytes;
    *b_offset = 0;
    *leftover_bytes -= *num_bytes;
    *num_bytes = BLOCKSIZE;
}

int handle_read(int i_num, void *buf, int size, int byte_offset, int pid)
{
    if (is_valid_inputs(i_num, buf, size, byte_offset) == ERROR)
    {
        return ERROR;
    }

    struct inode *inode = get_i(i_num);

    if (byte_offset > inode->size)
    {
        return ERROR;
    }

    int leftover_bytes = get_num_bytes_left(byte_offset, size, inode);
    int ret = leftover_bytes;

    int b_offset = byte_offset % BLOCKSIZE;
    int num_bytes = BLOCKSIZE - b_offset;

    int i;
    for (i = byte_offset / BLOCKSIZE; leftover_bytes > 0; i++)
    {
        int b_num = get_nth_b(inode, i, false);
        if (b_num == 0)
        {
            return ERROR;
        }
        void *cur_b = get_b(b_num);

        if (leftover_bytes < num_bytes)
        {
            num_bytes = leftover_bytes;
        }

        if (CopyTo(pid, buf, (char *)cur_b + b_offset, num_bytes) == ERROR)
        {
            TracePrintf(1, "ERROR: COPY %d BYTES FOR PROC %d\n", num_bytes, pid);
            return ERROR;
        }

        read_update(&buf, &b_offset, &leftover_bytes, &num_bytes);
    }

    return ret;
}

int is_valid_i(struct inode *inode)
{
    if (inode->type != INODE_REGULAR)
    {
        return false;
    }
    return true;
}

int get_num_copy_bytes(int leftover_bytes, int b_offset)
{
    int num_bytes = BLOCKSIZE - b_offset;
    if (leftover_bytes < num_bytes)
    {
        num_bytes = leftover_bytes;
    }
    return num_bytes;
}

void write_update(void **buf, int *b_offset, int *leftover_bytes, int *num_bytes)
{
    *buf += *num_bytes;
    *b_offset = 0;
    *leftover_bytes -= *num_bytes;
    *num_bytes = BLOCKSIZE;
}

void update_i_size(struct inode *inode, int size, int byte_offset, int leftover_bytes)
{
    int wrote_bytes = size - leftover_bytes;
    if (wrote_bytes + byte_offset > inode->size)
    {
        inode->size = wrote_bytes + byte_offset;
    }
}

int handle_write(int i_num, void *buf, int size, int byte_offset, int pid)
{
    struct inode *inode = get_i(i_num);

    if (is_valid_i(inode) == ERROR)
    {
        return ERROR;
    }

    int leftover_bytes = size;
    int ret = leftover_bytes;
    int b_offset = byte_offset % BLOCKSIZE;
    int num_bytes = get_num_copy_bytes(leftover_bytes, b_offset);

    int i;
    for (i = byte_offset / BLOCKSIZE; leftover_bytes > 0; i++)
    {
        int b_num = get_nth_b(inode, i, true);
        if (b_num == 0)
        {
            return ERROR;
        }
        void *cur_b = get_b(b_num);
        num_bytes = get_num_copy_bytes(leftover_bytes, b_offset);

        if (CopyFrom(pid, (char *)cur_b + b_offset, buf, num_bytes) == ERROR)
        {
            TracePrintf(1, "ERROR: DID NOT COPY %d BYTES FROM USER PID %d\n", num_bytes, pid);
            return ERROR;
        }

        write_update(&buf, &b_offset, &leftover_bytes, &num_bytes);

        // mark block as dirty
        cache_item *b_tmp = (cache_item *)query_ht(b_ht, b_num);
        b_tmp->dirty = true;

        update_i_size(inode, size, byte_offset, leftover_bytes);
    }

    cache_item *i_tmp = (cache_item *)query_ht(i_ht, i_num);
    i_tmp->dirty = true;

    return ret;
}

void adj_path_i(char **name, int *curr_i)
{
    if ((*name)[0] == '/')
    {
        *name += sizeof(char);
        *curr_i = ROOTINODE;
    }
}

int handle_link(char *old_name, char *new_name, int curr_i)
{
    if (old_name == NULL || new_name == NULL || curr_i <= 0)
    {
        return ERROR;
    }

    adj_path_i(&old_name, &curr_i);
    int old_i_name = get_path_i_num(old_name, curr_i);
    struct inode *inode = get_i(old_i_name);

    if (inode->type == INODE_DIRECTORY || old_i_name == 0)
    {
        return ERROR;
    }

    adj_path_i(&new_name, &curr_i);

    if (handle_create(new_name, curr_i, old_i_name) == ERROR)
    {
        return ERROR;
    }

    inode->nlink++;
    cache_item *i_tmp = (cache_item *)query_ht(i_ht, old_i_name);
    i_tmp->dirty = true;

    return 0;
}

int handle_unlink(char *pathname, int curr_i)
{
    if (pathname == NULL || curr_i <= 0)
    {
        return ERROR;
    }

    // Get the containing directory
    char *filename;
    int dir_i_num = get_super_dir(pathname, curr_i, &filename);

    struct inode *dir_i = get_i(dir_i_num);
    if (dir_i->type != INODE_DIRECTORY)
    {
        return ERROR;
    }

    int b_num;
    int offset = get_dir_entry(filename, dir_i_num, &b_num, false);
    if (offset == -1)
    {
        return ERROR;
    }
    void *block = get_b(b_num);

    // Get the directory entry associated with the path
    struct dir_entry *dir_entry = (struct dir_entry *)((char *)block + offset);

    // Get the inode associated with the directory entry
    int i_num = dir_entry->inum;
    struct inode *inode = get_i(i_num);

    // Decrease nlinks by 1
    inode->nlink--;

    // If nlinks == 0, clear the file
    if (inode->nlink == 0)
    {
        int i = 0;
        int b_num;
        while ((b_num = get_nth_b(inode, i++, false)) != 0)
        {
            // Add block to the free block list (originally from addfree_blockToList)
            free_block *new_b = malloc(sizeof(free_block));
            new_b->b_num = b_num;
            new_b->next_inode = b_head;
            b_head = new_b;
            free_b_count++;
        }
        inode->size = 0;
    }

    // Mark inode as dirty
    cache_item *i_tmp = (cache_item *)query_ht(i_ht, i_num);
    i_tmp->dirty = true;

    // Set the inum to zero
    dir_entry->inum = 0;

    // Mark block as dirty
    cache_item *b_tmp = (cache_item *)query_ht(b_ht, b_num);
    b_tmp->dirty = true;

    return 0;
}
int update_path_i(char **pathname, int *curr_i)
{
    if (*pathname == NULL || *curr_i <= 0)
    {
        return ERROR;
    }
    if ((*pathname)[0] == '/')
    {
        while ((*pathname)[0] == '/')
            *pathname += sizeof(char);
        *curr_i = ROOTINODE;
    }
    return 0;
}

void update_dir_entry(struct dir_entry *dir_entry, char *filename, int i_num)
{
    memset(&dir_entry->name, '\0', DIRNAMELEN);
    int i;
    for (i = 0; filename[i] != '\0'; i++)
    {
        dir_entry->name[i] = filename[i];
    }
    dir_entry->inum = i_num;
}

int handle_mkdir(char *pathname, int curr_i)
{
    if (update_path_i(&pathname, &curr_i) == ERROR)
    {
        return ERROR;
    }
    char *filename;
    int dir_i_num = get_super_dir(pathname, curr_i, &filename);
    int b_num;
    int offset = get_dir_entry(filename, dir_i_num, &b_num, true);
    void *block = get_b(b_num);

    struct dir_entry *dir_entry = (struct dir_entry *)((char *)block + offset);

    if (dir_entry->inum != 0)
    {
        return ERROR;
    }

    int i_num = get_next_inum_free();
    update_dir_entry(dir_entry, filename, i_num);
    cache_item *b_tmp = (cache_item *)query_ht(b_ht, b_num);
    b_tmp->dirty = true;

    block = get_b((i_num / (BLOCKSIZE / INODESIZE)) + 1);
    struct inode *inode = get_i(i_num);
    inode->type = INODE_DIRECTORY;
    inode->size = 2 * sizeof(struct dir_entry);
    inode->nlink = 1;

    int head_dir_b_num = get_next_free_bnum();
    void *head_dir_b = get_b(head_dir_b_num);
    inode->direct[0] = head_dir_b_num;

    struct dir_entry *dir1 = (struct dir_entry *)head_dir_b;
    dir1->inum = i_num;
    dir1->name[0] = '.';

    struct dir_entry *dir2 = (struct dir_entry *)((char *)dir1 + sizeof(struct dir_entry));
    dir2->inum = dir_i_num;
    dir2->name[0] = '.';
    dir2->name[1] = '.';

    // mark block as dirty
    b_tmp = (cache_item *)query_ht(b_ht, head_dir_b_num);
    b_tmp->dirty = true;

    cache_item *i_tmp = (cache_item *)query_ht(i_ht, i_num);
    i_tmp->dirty = true;

    return 0;
}

void clear_file(struct inode *inode)
{
    int i = 0;
    int b_num;
    while ((b_num = get_nth_b(inode, i++, false)) != 0)
    {
        free_block *new_b = malloc(sizeof(free_block));
        new_b->b_num = b_num;
        new_b->next_inode = b_head;

        b_head = new_b;
        free_b_count++;
    }
    inode->size = 0;
}

int handle_rmdir(char *pathname, int curr_i)
{
    // update the pathname and curr_i if necessary
    if (update_path_i(&pathname, &curr_i) == ERROR)
    {
        TracePrintf(1, "handle_rmdir: update_path_i returned an error\n");
        return ERROR;
    }

    // get the inode c_item_num for the given pathname and current inode
    int i_num = get_path_i_num(pathname, curr_i);

    if (i_num == ERROR)
    {
        TracePrintf(1, "handle_rmdir: invalid inode i_num for pathname '%s'\n", pathname);
        return ERROR;
    }

    struct inode *inode = get_i(i_num);

    if (inode->size > (int)(2 * sizeof(struct dir_entry)))
    {
        TracePrintf(1, "handle_rmdir: inode size is greater than 2 * sizeof(struct dir_entry)\n");
        return ERROR;
    }

    clear_file(inode);
    add_free_i_list(i_num);

    char *filename;
    int dir_i_num = get_super_dir(pathname, curr_i, &filename);

    int b_num;
    int offset = get_dir_entry(filename, dir_i_num, &b_num, true);

    // get the block containing the directory entry and update it
    void *block = get_b(b_num);
    struct dir_entry *dir_entry = (struct dir_entry *)((char *)block + offset);
    dir_entry->inum = 0;

    // mark the block as dirty
    cache_item *b_tmp = (cache_item *)query_ht(b_ht, b_num);
    b_tmp->dirty = true;

    TracePrintf(2, "handle_rmdir: remove directory successful for pathname '%s'\n", pathname);

    return 0;
}

int handle_chdir(char *pathname, int curr_i)
{
    // update the pathname and curr_i if necessary
    if (update_path_i(&pathname, &curr_i) == ERROR)
    {
        TracePrintf(1, "handle_chdir: getUpdatedPathAndInode returned an error\n");
        return ERROR;
    }

    int inode = get_path_i_num(pathname, curr_i);

    if (inode == 0)
    {
        TracePrintf(1, "handle_chdir: invalid inode c_item_num for pathname '%s'\n", pathname);
        return ERROR;
    }

    TracePrintf(2, "handle_chdir: change directory successful for pathname '%s'\n", pathname);

    return inode;
}

int handle_stat(char *pathname, int curr_i, struct Stat *stat_buffer, int pid)
{
    if (pathname == NULL || curr_i <= 0 || stat_buffer == NULL)
    {
        TracePrintf(2, "handle_stat: pathname, curr_i, or stat_buffer is NULL or invalid\n");
        return ERROR;
    }

    if (pathname[0] == '/')
    {
        // remove any leading '/' characters
        while (pathname[0] == '/')
            pathname += sizeof(char);

        // set the current inode to the root inode
        curr_i = ROOTINODE;
    }

    int i_num = get_path_i_num(pathname, curr_i);
    if (i_num == 0)
    {
        TracePrintf(2, "handle_stat: invalid inode c_item_num for pathname '%s'\n", pathname);
        return ERROR;
    }

    struct inode *inode = get_i(i_num);

    struct Stat stat;
    stat.inum = i_num;
    stat.nlink = inode->nlink;
    stat.size = inode->size;
    stat.type = inode->type;

    // copy the stat struct to the stat_buffer for the given pid
    if (CopyTo(pid, stat_buffer, &stat, sizeof(struct Stat)) == ERROR)
    {
        TracePrintf(1, "ERROR: COULD NOT COPY %d BYTE FOR PROCESS %d\n", sizeof(struct Stat), pid);
        return ERROR;
    }

    TracePrintf(2, "handle_stat: stat successful for pathname '%s'\n", pathname);

    return 0;
}

int handle_shutdown(void)
{
    handle_sync();
    TracePrintf(1, "SHUTTING DOWN YFS\n");
    Exit(0);
}
int handle_seek(int i_num, int offset, int whence, int cur_pos)
{
    struct inode *inode = get_i(i_num);
    int size = inode->size;
    // check if the given cur_pos is valid
    if (cur_pos > size || cur_pos < 0)
    {
        TracePrintf(2, "handle_seek: invalid current pos (%d)\n", cur_pos);
        return ERROR;
    }
    if (whence == SEEK_SET)
    {
        // check if the given offset is valid
        if (offset < 0 || offset > size)
        {
            TracePrintf(2, "handle_seek: invalid offset (%d) for SEEK_SET\n", offset);
            return ERROR;
        }
        // calculate the new pos and return it
        int new_pos = offset;
        TracePrintf(2, "handle_seek: moved to SEEK_SET pos %d\n", new_pos);
        return new_pos;
    }
    if (whence == SEEK_CUR)
    {
        int new_pos = cur_pos + offset;

        // check if the new pos is valid
        if (new_pos > size || new_pos < 0)
        {
            TracePrintf(2, "handle_seek: invalid new pos (%d) for SEEK_CUR\n", new_pos);
            return ERROR;
        }

        TracePrintf(2, "handle_seek: moved to SEEK_CUR pos %d\n", new_pos);
        return new_pos;
    }

    if (whence == SEEK_END)
    {
        int new_pos = size + offset;

        // check if the new pos is valid
        if (offset > 0 || new_pos < 0)
        {
            TracePrintf(2, "handle_seek: invalid new pos (%d) for SEEK_END\n", new_pos);
            return ERROR;
        }

        TracePrintf(2, "handle_seek: moved to SEEK_END pos %d\n", new_pos);
        return new_pos;
    }

    // if the given whence is not valid, return ERROR
    TracePrintf(2, "handle_seek: invalid whence value (%d)\n", whence);
    return ERROR;
}

int handle_sync(void)
{
    TracePrintf(1, "starting sync\n");

    // first, sync all dirty blocks
    cache_item *cur_b_tmp = cache_b_queue->q_first;
    while (cur_b_tmp != NULL)
    {
        // check if this block is dirty (i.e. has been modified)
        if (cur_b_tmp->dirty)
        {
            // write the block to disk
            WriteSector(cur_b_tmp->c_item_num, cur_b_tmp->addr);
            TracePrintf(2, "sync: wrote block %d to disk\n", cur_b_tmp->c_item_num);
        }

        // move on to the next_inode block in the queue
        cur_b_tmp = cur_b_tmp->next_c_item;
    }

    // now, sync all dirty inodes
    cache_item *cur_i_tmp = cache_i_queue->q_first;
    while (cur_i_tmp != NULL)
    {
        // check if this inode is dirty (i.e. has been modified)
        if (cur_i_tmp->dirty)
        {
            // get the block containing this inode
            int i_num = cur_i_tmp->c_item_num;
            int b_num = (i_num / (BLOCKSIZE / INODESIZE)) + 1;

            // copy the inode from the cache to the block
            void *block = get_b(b_num);
            void *i_block_addr = (block + (i_num - (b_num - 1) * (BLOCKSIZE / INODESIZE)) * INODESIZE);
            memcpy(i_block_addr, cur_i_tmp->addr, sizeof(struct inode));

            // write the block containing the inode to disk
            WriteSector(b_num, block);
            TracePrintf(2, "sync: wrote inode %d (in block %d) to disk\n", i_num, b_num);
        }

        // move on to the next_inode inode in the queue
        cur_i_tmp = cur_i_tmp->next_c_item;
    }

    TracePrintf(1, "finished with sync()\n");
    return 0;
}


/** HASH TABLE HELPER BEGIN */

int insert_ht(struct ht *ht, int key, void *value)
{
    ht_map *elem;
    unsigned int index;

    assert(query_ht(ht, key) == NULL);
    assert(value != NULL);

    elem = malloc(sizeof(ht_map));
    if (elem == NULL)
    {

        return (-1);
    }
    elem->key = key;
    elem->value = value;

    index = (unsigned int)(key % ht->size) % ht->size;

    elem->next_inode = ht->head[index];
    ht->head[index] = elem;
    ht->num_used++;
    return (0);
}

void evict_ht(struct ht *ht, int key)
{
    ht_map *elem, *prev;
    unsigned int index;

    index = (unsigned int)(key % ht->size) % ht->size;

    for (elem = ht->head[index]; elem != NULL; elem = elem->next_inode)
    {

        if (elem->key == key)
        {

            if (elem == ht->head[index])

            {

                ht->head[index] = elem->next_inode;
            }
            else
            {
                prev->next_inode = elem->next_inode;
            }
            ht->num_used--;

            free(elem);

            return;
        }
        prev = elem;
    }

    return;
}

void *
query_ht(struct ht *ht, int key)
{
    ht_map *elem;
    unsigned int index;

    index = (unsigned int)(key % ht->size) % ht->size;

    for (elem = ht->head[index]; elem != NULL; elem = elem->next_inode)
    {

        if (elem->key == key)
            return (elem->value);
    }

    return (NULL);
}

/** REGULAR HELPER FUNCTIONS BEGIN*/
char *
get_proc_path(int pid, char *pathname, int len)
{
    char *local_pathname = malloc(len * sizeof(char));
    if (local_pathname == NULL)
    {
        return NULL;
    }
    if (CopyFrom(pid, local_pathname, pathname, len) != 0)
    {
        return NULL;
    }
    return local_pathname;
}


cache_item *left_dequeue(queue *queue)
{
    cache_item *q_first = queue->q_first;
    if (q_first == NULL)
    {
        return NULL;
    }
    if (queue->q_first->next_c_item == NULL)
    {
        queue->q_last = NULL;
    }

    queue->q_first->prev_c_item = NULL;
    queue->q_first = queue->q_first->next_c_item;

    if (queue->q_first != NULL)
    {
        queue->q_first->prev_c_item = NULL;
    }

    return q_first;
}

void right_dequeue(queue *queue, cache_item *item)
{
    if (item->prev_c_item == NULL)
    {
        left_dequeue(queue);
    }
    else
    {
        if (item->next_c_item == NULL)
        {
            queue->q_last = item->prev_c_item;
        }
        item->prev_c_item->next_c_item = item->next_c_item;
        if (item->next_c_item != NULL)
        {
            item->next_c_item->prev_c_item = item->prev_c_item;
        }
    }
}

void right_enqueue(cache_item *item, queue *queue)
{
    // if the queue is empty
    if (queue->q_first == NULL)
    {
        if (queue == cache_b_queue)
            item->next_c_item = NULL;
        item->prev_c_item = NULL;
        queue->q_last = item;
        queue->q_first = item;
    }
    else
    { // if the queue is nonempty
        queue->q_last->next_c_item = item;
        item->prev_c_item = queue->q_last;
        queue->q_last = item;
        queue->q_last->next_c_item = NULL;
    }
}

// Helper function to evict LRU block from cache
void evict_b()
{
    // cache full, evict lru block
    // get block num
    // write block back to disk
    // remove block from block hash table
    // Free the memory occupied by the LRU block item

    cache_item *lrub_tmp = left_dequeue(cache_b_queue);
    int lru_b_num = lrub_tmp->c_item_num;
    WriteSector(lru_b_num, lrub_tmp->addr);
    cache_b_size--;
    evict_ht(b_ht, lru_b_num);

    // Free the memory occupied by the LRU block item
    free(lrub_tmp->addr);
    free(lrub_tmp);
}

// Get LRU block from cache
void *get_b(int b_num)
{
    // find block in hash table
    cache_item *b_tmp = (cache_item *)query_ht(b_ht, b_num);

    // if block is found, update pos to end of queue
    if (b_tmp != NULL)
    {
        right_dequeue(cache_b_queue, b_tmp);
        right_enqueue(b_tmp, cache_b_queue);
        return b_tmp->addr;
    }

    // if cache is full, evict LRU block
    if (cache_b_size == BLOCK_CACHESIZE)
    {
        evict_b();
    }

    // otherwise, malloc and read block from disk
    void *block = malloc(BLOCKSIZE);
    ReadSector(b_num, block);

    // create a new cache item to add to the LRU cache
    cache_item *c_tmp = malloc(sizeof(cache_item));
    c_tmp->c_item_num = b_num;
    c_tmp->addr = block;
    c_tmp->dirty = false;
    right_enqueue(c_tmp, cache_b_queue);
    cache_b_size++;
    insert_ht(b_ht, b_num, c_tmp);

    return block;
}

// evict the LRU inode from the cache
void evict_i()
{
    cache_item *lru_i = left_dequeue(cache_i_queue);
    int lru_i_num = lru_i->c_item_num;
    cache_i_size--;
    evict_ht(i_ht, lru_i_num);

    // write the evicted inode back to its block
    int lru_b_num = (lru_i_num / (BLOCKSIZE / INODESIZE)) + 1;
    void *lru_b = get_b(lru_b_num);
    void *i_block_addr = (lru_b + (lru_i_num - (lru_b_num - 1) * (BLOCKSIZE / INODESIZE)) * INODESIZE);
    memcpy(i_block_addr, lru_i->addr, sizeof(struct inode));

    // mark block as dirty
    cache_item *tmpb_tmp = (cache_item *)query_ht(b_ht, lru_b_num);
    tmpb_tmp->dirty = true;

    // free the memory occupied by the LRU inode item
    free(lru_i->addr);
    free(lru_i);
}

// Get LRU inode from cache
struct inode *get_i(int i_num)
{
    cache_item *c_i_tmp = (cache_item *)query_ht(i_ht, i_num);

    // if iniode found, update pos to end of queue
    if (c_i_tmp != NULL)
    {
        right_dequeue(cache_i_queue, c_i_tmp);
        right_enqueue(c_i_tmp, cache_i_queue);
        return c_i_tmp->addr;
    }

    // cache full, evict LRU inode
    if (cache_i_size == INODE_CACHESIZE)
    {
        evict_i();
    }

    // read the requested inode from its block
    int b_num = (i_num / (BLOCKSIZE / INODESIZE)) + 1;
    void *b_addr = get_b(b_num);
    struct inode *new_i_b_addr = (struct inode *)(b_addr + (i_num - (b_num - 1) * (BLOCKSIZE / INODESIZE)) * INODESIZE);

    // copy the inode from its block to a new memory location and create a new cache item for it
    struct inode *i_cpy = malloc(sizeof(struct inode));
    struct cache_item *i_tmp = malloc(sizeof(struct cache_item));
    memcpy(i_cpy, new_i_b_addr, sizeof(struct inode));
    i_tmp->addr = i_cpy;
    i_tmp->c_item_num = i_num;

    // add the new inode cache item to the LRU queue and the inode hash table
    right_enqueue(i_tmp, cache_i_queue);
    cache_i_size++;
    insert_ht(i_ht, i_num, i_tmp);

    return i_tmp->addr;
}

// get the nth block of an inode. If the block does not exist and allocateIfNeeded is true, allocate a new block.
int get_nth_b(struct inode *inode, int n, bool allocateIfNeeded)
{
    bool finish = false;

    // check if n exceeds the maximum c_item_num of blocks allowed for an inode
    if (n >= NUM_DIRECT + BLOCKSIZE / (int)sizeof(int))
    {
        return 0;
    }

    // check if the requested block is outside the current file size
    if (n * BLOCKSIZE >= inode->size)
    {
        finish = true;
    }

    // if the requested block is outside the current file size and allocation is not needed, return 0
    if (finish && !allocateIfNeeded)
    {
        return 0;
    }

    // if the requested block is within the direct block pointers range
    if (n < NUM_DIRECT)
    {
        // if the requested block is outside the current file size, allocate a new block
        if (finish)
        {
            inode->direct[n] = get_next_free_bnum();
        }
        return inode->direct[n];
    }

    // if the requested block is within the indirect block pointers range
    // get the indirect block
    int *b_indir = get_b(inode->indirect);

    // if the requested block is outside the current file size, allocate a new block
    if (finish)
    {
        b_indir[n - NUM_DIRECT] = get_next_free_bnum();
    }

    // get the block c_item_num from the indirect block
    int b_num = b_indir[n - NUM_DIRECT];

    // return the block c_item_num
    return b_num;
}

// Helper function to get the next_inode path segment
char *get_next_path_part(char *path)
{
    while (*path != '/' && *path != '\0')
    {
        path++;
    }

    while (*path == '/')
    {
        path++;
    }

    return path;
}

// Get the inode c_item_num for the given path, starting from the specified inode c_item_num
// This function searches for the inode c_item_num of a given path starting from a given inode c_item_num in the file system.
// It recursively navigates through the directory structure until it finds the inode for the given path.

int get_path_i_num(char *path, int start_i)
{
    int next_i_num = 0;
    void *block;
    struct inode *inode = get_i(start_i);

    if (inode->type == INODE_DIRECTORY)
    {
        int b_num;
        int offset = get_dir_entry(path, start_i, &b_num, false);
        if (offset != -1)
        {
            block = get_b(b_num);
            struct dir_entry *dir_entry = (struct dir_entry *)((char *)block + offset);
            // set next_inode inode c_item_num to inode c_item_num of the directory entry
            next_i_num = dir_entry->inum;
        }
    }
    else if (inode->type == INODE_REGULAR || next_i_num == 0)
    {
        return 0;
    }

    char *p_next = get_next_path_part(path);
    // check if we've reached the end of the path
    if (*p_next == '\0')
    {
        // get inode pointer using next_i_num
        inode = get_i(next_i_num);
        return next_i_num;
    }

    // recursively call function with next_inode path segment and next_inode inode c_item_num
    return get_path_i_num(p_next, next_i_num);
}

// grab the next_inode free inode c_item_num from inode freelist
int get_next_inum_free()
{
    if (i_head == NULL)
    {
        return 0;
    }

    int i_num = i_head->i_num;
    struct inode *inode = get_i(i_num);
    inode->reuse++; // now we have used this inode

    // mark dirty
    cache_item *i_tmp = (cache_item *)query_ht(i_ht, i_num);
    i_tmp->dirty = true;

    // Move the first free inode pointer to the next_inode free inode
    i_head = i_head->next_inode;

    return i_num;
}

void add_free_i_list(int i_num)
{
    free_inode *new_b = malloc(sizeof(free_inode));
    new_b->i_num = i_num;
    new_b->next_inode = i_head;
    i_head = new_b;
    free_i_count++;
}

int get_next_free_bnum()
{
    if (b_head == NULL)
    {
        return 0;
    }
    int b_num = b_head->b_num;
    b_head = b_head->next_inode;
    return b_num;
}

void add_b_freelist(int b_num)
{
    free_block *new_b = malloc(sizeof(free_block));
    new_b->b_num = b_num;
    new_b->next_inode = b_head;
    b_head = new_b;
    free_b_count++;
}

// initializes free inode and block lists for yfs

void make_free_lists()
{
 // malloc inode and block queues
    cache_i_queue = malloc(sizeof(queue));
    cache_i_queue->q_first = NULL;
    cache_i_queue->q_last = NULL;

    cache_b_queue = malloc(sizeof(queue));
    cache_b_queue->q_first = NULL;
    cache_b_queue->q_last = NULL;

    // init hash tables for inodes and blocks
    double load_factor = 1.5;
    int inode_size = INODE_CACHESIZE + 1;
    int block_size = BLOCK_CACHESIZE + 1;

    i_ht = malloc(sizeof(struct ht));
    if (i_ht == NULL) {
        perror("malloc failed for i_ht");
        exit(1);
    }
    i_ht->head = calloc(inode_size, sizeof(ht_map *));
    if (i_ht->head == NULL) {
        perror("calloc failed for i_ht->head");
        free(i_ht);
        exit(1);
    }
    i_ht->size = inode_size;
    i_ht->num_used = 0;
    i_ht->load_factor = load_factor;

    b_ht = malloc(sizeof(struct ht));
    if (b_ht == NULL) {
        perror("malloc failed for b_ht");
        free(i_ht->head);
        free(i_ht);
        exit(1);
    }
    b_ht->head = calloc(block_size, sizeof(ht_map *));
    if (b_ht->head == NULL) {
        perror("calloc failed for b_ht->head");
        free(b_ht);
        free(i_ht->head);
        free(i_ht);
        exit(1);
    }
    b_ht->size = block_size;
    b_ht->num_used = 0;
    b_ht->load_factor = load_factor;

    // init variables for loop
    int b_num = 1;
    void *block = get_b(b_num);

    // init fs header and initialize array of taken blocks
    struct fs_header header = *((struct fs_header *)block);
    bool used_b_list[header.num_blocks];
    memset(used_b_list, false, header.num_blocks * sizeof(bool));
    used_b_list[0] = true;

    int i_num = ROOTINODE;
    // loop through all inodes
    while (i_num < header.num_inodes)
    {
        // loop through inodes in current block
        while (i_num < (BLOCKSIZE / INODESIZE) * b_num)
        {
            struct inode *inode = get_i(i_num);
            if (inode->type == INODE_FREE)
            {
                // if inode is free, create free_inode and add it to freelist
                free_inode *new_b = malloc(sizeof(free_inode));
                new_b->i_num = i_num;
                new_b->next_inode = i_head;
                i_head = new_b;
                free_i_count++;
            }
            else
            {
                // if inode is not free, mark the blocks it uses as taken
                int i = 0;
                int b_num;
                while (1)
                {
                    // loop through all direct and indirect block pointers in inode
                    if (i >= NUM_DIRECT + BLOCKSIZE / (int)sizeof(int))
                    {
                        break;
                    }
                    if (i < NUM_DIRECT)
                    {
                        b_num = inode->direct[i];
                    }
                    else
                    {
                        int *b_indir = get_b(inode->indirect);
                        b_num = b_indir[i - NUM_DIRECT];
                    }
                    // mark block as taken
                    if (b_num == 0)
                    {
                        break;
                    }
                    used_b_list[b_num] = true;
                    i++;
                }
            }
            i_num++;
        }
        b_num++;
        block = get_b(b_num);
    }

    // loop through all blocks and add free blocks to list
    int i;
    for (i = 0; i < header.num_blocks; i++)
    {
        if (!used_b_list[i])
        {
            free_block *new_b = malloc(sizeof(free_block));
            new_b->b_num = i;
            new_b->next_inode = b_head;
            b_head = new_b;
            free_b_count++;
        }
    }
}

// This function searches for a directory entry in a given inode for the specified pathname.
// If should_make_dir is true and the directory entry is not found, it creates a new directory entry.

int get_dir_entry(char *pathname, int start_i, int *b_numPtr, bool should_make_dir)
{
    // init variables for loop
    int entry_offset = -1;
    int entry_b_num = 0;
    void *cur_b;
    struct dir_entry *cur_entry;
    struct inode *inode = get_i(start_i);
    int i = 0;
    int b_num = get_nth_b(inode, i, false);
    int curr_b_num  = 0;
    int total_size = sizeof(struct dir_entry);
    bool found = false;

    // loop through all blocks in inode until the directory entry is found or all blocks have been searched
    while (b_num != 0 && !found)
    {
        cur_b = get_b(b_num);
        cur_entry = (struct dir_entry *)cur_b;
        while (total_size <= inode->size && ((char *)cur_entry < ((char *)cur_b + BLOCKSIZE)))
        {
            // if a free directory entry is found, remember its offset and block c_item_num
            if (entry_offset == -1 && cur_entry->inum == 0)
            {
                entry_b_num = b_num;
                entry_offset = (int)((char *)cur_entry - (char *)cur_b);
            }

            // compare the directory entry name with specified pathname
            bool eq = true;
            int j = 0;
            while (j < DIRNAMELEN)
            {
                if ((pathname[j] == '/' || pathname[j] == '\0') && cur_entry->name[j] == '\0')
                {
                    eq = true;
                    break;
                }
                if (pathname[j] != cur_entry->name[j])
                {
                    eq = false;
                    break;
                }
                j++;
            }

            // if the names are equal, we've found the directory entry
            if (eq)
            {
                found = true;
                break;
            }

            // increment the current entry and total size
            cur_entry = (struct dir_entry *)((char *)cur_entry + sizeof(struct dir_entry));
            total_size += sizeof(struct dir_entry);
        }
        if (found)
        {
            break;
        }
        curr_b_num  = b_num;
        b_num = get_nth_b(inode, ++i, false);
    }

    // set the block c_item_num pointer to the current block c_item_num
    *b_numPtr = b_num;

    // if the directory entry was found, return its offset
    if (found)
    {
        int offset = (int)((char *)cur_entry - (char *)cur_b);
        return offset;
    }

    // if we need to create the directory entry and there's free directory entry, use it
    if (should_make_dir)
    {
        if (entry_b_num != 0)
        {
            *b_numPtr = entry_b_num;
            return entry_offset;
        }

        // if there's no free directory entry, create new block and directory entry
        if (inode->size % BLOCKSIZE == 0)
        {
            b_num = get_nth_b(inode, i, true);
            cur_b = get_b(b_num);
            inode->size += sizeof(struct dir_entry);
            struct dir_entry *tmp_entry = (struct dir_entry *)cur_b;
            tmp_entry->inum = 0;

            // mark the block and inode dirty in the cache
            cache_item *b_tmp = (cache_item *)query_ht(b_ht, b_num);
            b_tmp->dirty = true;

            cache_item *i_tmp = (cache_item *)query_ht(i_ht, start_i);
            i_tmp->dirty = true;

            *b_numPtr = b_num;
            return 0;
        }

        // if the current block still has space, add a new directory entry to it
        inode->size += sizeof(struct dir_entry);
        cache_item *i_tmp = (cache_item *)query_ht(i_ht, start_i);
        i_tmp->dirty = true;
        cur_entry->inum = 0;

        // mark the current block and previous block dirty in the cache
        cache_item *b_tmp = (cache_item *)query_ht(b_ht, curr_b_num );
        b_tmp->dirty = true;
        cache_item *tmpb_tmp = (cache_item *)query_ht(b_ht, curr_b_num );
        tmpb_tmp->dirty = true;

        *b_numPtr = curr_b_num ;
        int offset = (int)((char *)cur_entry - (char *)cur_b);
        return offset;
    }

    // if the directory entry was not found and we're not creating one, return -1
    return -1;
}

// This function takes in a pathname and the inode c_item_num of the current directory.
// It returns the inode c_item_num of the directory that contains the file specified by pathname.
// It also sets the filename pointer to point to the filename in the pathname.

int get_super_dir(char *pathname, int curr_i, char **file_ptr)
{
    // check that the pathname is not too long
    int i;
    for (i = 0; i < MAXPATHNAMELEN; i++)
    {
        if (pathname[i] == '\0')
        {
            break;
        }
    }
    if (i == MAXPATHNAMELEN)
    {
        return ERROR;
    }

    // if the pathname starts with /, set curr_i to the root inode
    if (pathname[0] == '/')
    {
        while (pathname[0] == '/')
            pathname += sizeof(char);
        curr_i = ROOTINODE;
    }

    // find the index of last slash in the pathname
    int idx = 0;
    i = 0;
    while ((pathname[i] != '\0') && (i < MAXPATHNAMELEN))
    {
        if (pathname[i] == '/')
        {
            idx = i;
        }
        i++;
    }

    // if there is a slash in the pathname, get the containing directory's inode c_item_num and set the filename pointer
    if (idx != 0)
    {
        char path[idx + 1];
        for (i = 0; i < idx; i++)
        {
            path[i] = pathname[i];
        }
        path[i] = '\0';

        char *filename = pathname + (sizeof(char) * (idx + 1));
        *file_ptr = filename;
        int dir_i_num = get_path_i_num(path, curr_i);
        if (dir_i_num == 0)
        {
            return ERROR;
        }
        return dir_i_num;
    }
    // if there is no slash in the pathname, set the filename pointer and return the current directory's inode c_item_num
    else
    {
        *file_ptr = pathname;
        return curr_i;
    }
}
