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

                struct m_normal tmp_received;

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

                
                struct m_normal msg_rply;
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

int handle_create(char *pathname, int curr_i, int inodeNumToSet)
{
    if (!isPathnameValid(pathname) || curr_i <= 0)
    {
        return ERROR;
    }

    TracePrintf(1, "Creating %s in %d\n", pathname, curr_i);

    char *filename;
    int dirInodeNum = get_super_dir(pathname, curr_i, &filename);
    if (dirInodeNum == ERROR)
    {
        return ERROR;
    }

    struct inode *dirInode = get_i(dirInodeNum);
    if (dirInode->type != INODE_DIRECTORY)
    {
        return ERROR;
    }

    int inodeNum = get_create_file(filename, dirInodeNum, inodeNumToSet);
    if (inodeNum == ERROR)
    {
        return ERROR;
    }

    return inodeNum;
}

int get_create_file(char *filename, int dirInodeNum, int inodeNumToSet)
{
    int blockNum, offset;
    offset = get_dir_entry(filename, dirInodeNum, &blockNum, true);
    void *block = get_b(blockNum);
    struct dir_entry *dir_entry = (struct dir_entry *)((char *)block + offset);
    int inodeNum = dir_entry->inum;
    if (inodeNum != 0)
    {
        if (inodeNumToSet != -1)
        {
            return ERROR;
        }
        zero_file(inodeNum);
    }
    else
    {
        inodeNum = make_file(filename, dir_entry, inodeNumToSet);
    }
    cache_item *blockItem = (cache_item *)query_ht(b_ht, blockNum);
    blockItem->dirty = true;
    return inodeNum;
}

void zero_file(int inodeNum)
{
    struct inode *inode = get_i(inodeNum);
    int j = 0, blockNum;
    while ((blockNum = get_nth_b(inode, j++, false)) != 0)
    {
        free_block *newHead = malloc(sizeof(free_block));
        newHead->b_num = blockNum;
        newHead->next_inode = b_head;
        b_head = newHead;
        free_b_count++;
    }
    inode->size = 0;
    cache_item *inodeItem = (cache_item *)query_ht(i_ht, inodeNum);
    inodeItem->dirty = true;
}

int make_file(char *filename, struct dir_entry *dir_entry, int inodeNumToSet)
{
    strncpy(dir_entry->name, filename, DIRNAMELEN);
    //dont create new
    if (inodeNumToSet == -1)
    {
        int inodeNum = get_next_inum_free();
        dir_entry->inum = inodeNum;
        struct inode *inode = get_i(inodeNum);
        inode->type = INODE_REGULAR;
        inode->size = 0;
        inode->nlink = 1;
        cache_item *inodeItem = (cache_item *)query_ht(i_ht, inodeNum);
        inodeItem->dirty = true;
        return inodeNum;
    }
    else
    {
        dir_entry->inum = inodeNumToSet;
        return inodeNumToSet;
    }
}

int is_valid_inputs(int inodeNum, void *buf, int size, int byteOffset)
{
    if (buf == NULL || size < 0 || byteOffset < 0 || inodeNum <= 0)
    {
        return false;
    }
    return true;
}

int get_num_bytes_left(int byteOffset, int size, struct inode *inode)
{
    int bytesLeft = size;
    if (inode->size - byteOffset < size)
    {
        bytesLeft = inode->size - byteOffset;
    }
    return bytesLeft;
}

void read_update(void **buf, int *blockOffset, int *bytesLeft, int *bytesToCopy)
{
    *buf += *bytesToCopy;
    *blockOffset = 0;
    *bytesLeft -= *bytesToCopy;
    *bytesToCopy = BLOCKSIZE;
}

int handle_read(int inodeNum, void *buf, int size, int byteOffset, int pid)
{
    if (is_valid_inputs(inodeNum, buf, size, byteOffset) == ERROR)
    {
        return ERROR;
    }

    struct inode *inode = get_i(inodeNum);

    if (byteOffset > inode->size)
    {
        return ERROR;
    }

    int bytesLeft = get_num_bytes_left(byteOffset, size, inode);
    int returnVal = bytesLeft;

    int blockOffset = byteOffset % BLOCKSIZE;
    int bytesToCopy = BLOCKSIZE - blockOffset;

    int i;
    for (i = byteOffset / BLOCKSIZE; bytesLeft > 0; i++)
    {
        int blockNum = get_nth_b(inode, i, false);
        if (blockNum == 0)
        {
            return ERROR;
        }
        void *currentBlock = get_b(blockNum);

        if (bytesLeft < bytesToCopy)
        {
            bytesToCopy = bytesLeft;
        }

        if (CopyTo(pid, buf, (char *)currentBlock + blockOffset, bytesToCopy) == ERROR)
        {
            TracePrintf(1, "error copying %d bytes to pid %d\n", bytesToCopy, pid);
            return ERROR;
        }

        read_update(&buf, &blockOffset, &bytesLeft, &bytesToCopy);
    }

    return returnVal;
}

int is_valid_i(struct inode *inode)
{
    if (inode->type != INODE_REGULAR)
    {
        return false;
    }
    return true;
}

int get_num_copy_bytes(int bytesLeft, int blockOffset)
{
    int bytesToCopy = BLOCKSIZE - blockOffset;
    if (bytesLeft < bytesToCopy)
    {
        bytesToCopy = bytesLeft;
    }
    return bytesToCopy;
}

void write_update(void **buf, int *blockOffset, int *bytesLeft, int *bytesToCopy)
{
    *buf += *bytesToCopy;
    *blockOffset = 0;
    *bytesLeft -= *bytesToCopy;
    *bytesToCopy = BLOCKSIZE;
}

void update_i_size(struct inode *inode, int size, int byteOffset, int bytesLeft)
{
    int bytesWrittenSoFar = size - bytesLeft;
    if (bytesWrittenSoFar + byteOffset > inode->size)
    {
        inode->size = bytesWrittenSoFar + byteOffset;
    }
}

int handle_write(int inodeNum, void *buf, int size, int byteOffset, int pid)
{
    struct inode *inode = get_i(inodeNum);

    if (is_valid_i(inode) == ERROR)
    {
        return ERROR;
    }

    int bytesLeft = size;
    int returnVal = bytesLeft;
    int blockOffset = byteOffset % BLOCKSIZE;
    int bytesToCopy = get_num_copy_bytes(bytesLeft, blockOffset);

    int i;
    for (i = byteOffset / BLOCKSIZE; bytesLeft > 0; i++)
    {
        int blockNum = get_nth_b(inode, i, true);
        if (blockNum == 0)
        {
            return ERROR;
        }
        void *currentBlock = get_b(blockNum);
        bytesToCopy = get_num_copy_bytes(bytesLeft, blockOffset);

        if (CopyFrom(pid, (char *)currentBlock + blockOffset, buf, bytesToCopy) == ERROR)
        {
            TracePrintf(1, "ERROR: DID NOT COPY %d BYTES FROM USER PID %d\n", bytesToCopy, pid);
            return ERROR;
        }

        write_update(&buf, &blockOffset, &bytesLeft, &bytesToCopy);

        // mark block as dirty
        cache_item *blockItem = (cache_item *)query_ht(b_ht, blockNum);
        blockItem->dirty = true;

        update_i_size(inode, size, byteOffset, bytesLeft);
    }

    cache_item *inodeItem = (cache_item *)query_ht(i_ht, inodeNum);
    inodeItem->dirty = true;

    return returnVal;
}

void adjustPathAndInode(char **name, int *curr_i)
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

    adjustPathAndInode(&old_name, &curr_i);
    int old_nameNodeNum = get_path_i_num(old_name, curr_i);
    struct inode *inode = get_i(old_nameNodeNum);

    if (inode->type == INODE_DIRECTORY || old_nameNodeNum == 0)
    {
        return ERROR;
    }

    adjustPathAndInode(&new_name, &curr_i);

    if (handle_create(new_name, curr_i, old_nameNodeNum) == ERROR)
    {
        return ERROR;
    }

    inode->nlink++;
    cache_item *inodeItem = (cache_item *)query_ht(i_ht, old_nameNodeNum);
    inodeItem->dirty = true;

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
    int dirInodeNum = get_super_dir(pathname, curr_i, &filename);

    struct inode *dirInode = get_i(dirInodeNum);
    if (dirInode->type != INODE_DIRECTORY)
    {
        return ERROR;
    }

    int blockNum;
    int offset = get_dir_entry(filename, dirInodeNum, &blockNum, false);
    if (offset == -1)
    {
        return ERROR;
    }
    void *block = get_b(blockNum);

    // Get the directory entry associated with the path
    struct dir_entry *dir_entry = (struct dir_entry *)((char *)block + offset);

    // Get the inode associated with the directory entry
    int inodeNum = dir_entry->inum;
    struct inode *inode = get_i(inodeNum);

    // Decrease nlinks by 1
    inode->nlink--;

    // If nlinks == 0, clear the file
    if (inode->nlink == 0)
    {
        int i = 0;
        int blockNum;
        while ((blockNum = get_nth_b(inode, i++, false)) != 0)
        {
            // Add block to the free block list (originally from addfree_blockToList)
            free_block *newHead = malloc(sizeof(free_block));
            newHead->b_num = blockNum;
            newHead->next_inode = b_head;
            b_head = newHead;
            free_b_count++;
        }
        inode->size = 0;
    }

    // Mark inode as dirty
    cache_item *inodeItem = (cache_item *)query_ht(i_ht, inodeNum);
    inodeItem->dirty = true;

    // Set the inum to zero
    dir_entry->inum = 0;

    // Mark block as dirty
    cache_item *blockItem = (cache_item *)query_ht(b_ht, blockNum);
    blockItem->dirty = true;

    return 0;
}
int getUpdatedPathAndInode(char **pathname, int *curr_i)
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

void updateDirectoryEntry(struct dir_entry *dir_entry, char *filename, int inodeNum)
{
    memset(&dir_entry->name, '\0', DIRNAMELEN);
    int i;
    for (i = 0; filename[i] != '\0'; i++)
    {
        dir_entry->name[i] = filename[i];
    }
    dir_entry->inum = inodeNum;
}

int handle_mkdir(char *pathname, int curr_i)
{
    if (getUpdatedPathAndInode(&pathname, &curr_i) == ERROR)
    {
        return ERROR;
    }
    char *filename;
    int dirInodeNum = get_super_dir(pathname, curr_i, &filename);
    int blockNum;
    int offset = get_dir_entry(filename, dirInodeNum, &blockNum, true);
    void *block = get_b(blockNum);

    struct dir_entry *dir_entry = (struct dir_entry *)((char *)block + offset);

    if (dir_entry->inum != 0)
    {
        return ERROR;
    }

    int inodeNum = get_next_inum_free();
    updateDirectoryEntry(dir_entry, filename, inodeNum);
    cache_item *blockItem = (cache_item *)query_ht(b_ht, blockNum);
    blockItem->dirty = true;

    block = get_b((inodeNum / (BLOCKSIZE / INODESIZE)) + 1);
    struct inode *inode = get_i(inodeNum);
    inode->type = INODE_DIRECTORY;
    inode->size = 2 * sizeof(struct dir_entry);
    inode->nlink = 1;

    int firstDirectBlockNum = get_next_free_bnum();
    void *firstDirectBlock = get_b(firstDirectBlockNum);
    inode->direct[0] = firstDirectBlockNum;

    struct dir_entry *dir1 = (struct dir_entry *)firstDirectBlock;
    dir1->inum = inodeNum;
    dir1->name[0] = '.';

    struct dir_entry *dir2 = (struct dir_entry *)((char *)dir1 + sizeof(struct dir_entry));
    dir2->inum = dirInodeNum;
    dir2->name[0] = '.';
    dir2->name[1] = '.';

    // mark block as dirty
    blockItem = (cache_item *)query_ht(b_ht, firstDirectBlockNum);
    blockItem->dirty = true;

    cache_item *inodeItem = (cache_item *)query_ht(i_ht, inodeNum);
    inodeItem->dirty = true;

    return 0;
}

void clearFile(struct inode *inode)
{
    int i = 0;
    int blockNum;
    while ((blockNum = get_nth_b(inode, i++, false)) != 0)
    {
        free_block *newHead = malloc(sizeof(free_block));
        newHead->b_num = blockNum;
        newHead->next_inode = b_head;

        b_head = newHead;
        free_b_count++;
    }
    inode->size = 0;
}

int handle_rmdir(char *pathname, int curr_i)
{
    // update the pathname and curr_i if necessary
    if (getUpdatedPathAndInode(&pathname, &curr_i) == ERROR)
    {
        TracePrintf(1, "handle_rmdir: getUpdatedPathAndInode returned an error\n");
        return ERROR;
    }

    // get the inode c_item_num for the given pathname and current inode
    int inodeNum = get_path_i_num(pathname, curr_i);

    if (inodeNum == ERROR)
    {
        TracePrintf(1, "handle_rmdir: invalid inode c_item_num for pathname '%s'\n", pathname);
        return ERROR;
    }

    struct inode *inode = get_i(inodeNum);

    if (inode->size > (int)(2 * sizeof(struct dir_entry)))
    {
        TracePrintf(1, "handle_rmdir: inode size is greater than 2 * sizeof(struct dir_entry)\n");
        return ERROR;
    }

    clearFile(inode);
    add_free_i_list(inodeNum);

    char *filename;
    int dirInodeNum = get_super_dir(pathname, curr_i, &filename);

    int blockNum;
    int offset = get_dir_entry(filename, dirInodeNum, &blockNum, true);

    // get the block containing the directory entry and update it
    void *block = get_b(blockNum);
    struct dir_entry *dir_entry = (struct dir_entry *)((char *)block + offset);
    dir_entry->inum = 0;

    // mark the block as dirty
    cache_item *blockItem = (cache_item *)query_ht(b_ht, blockNum);
    blockItem->dirty = true;

    TracePrintf(2, "handle_rmdir: remove directory successful for pathname '%s'\n", pathname);

    return 0;
}

int handle_chdir(char *pathname, int curr_i)
{
    // update the pathname and curr_i if necessary
    if (getUpdatedPathAndInode(&pathname, &curr_i) == ERROR)
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

    int inodeNum = get_path_i_num(pathname, curr_i);
    if (inodeNum == 0)
    {
        TracePrintf(2, "handle_stat: invalid inode c_item_num for pathname '%s'\n", pathname);
        return ERROR;
    }

    struct inode *inode = get_i(inodeNum);

    struct Stat stat;
    stat.inum = inodeNum;
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
int handle_seek(int inodeNum, int offset, int whence, int currentPosition)
{
    struct inode *inode = get_i(inodeNum);
    int size = inode->size;
    // check if the given currentPosition is valid
    if (currentPosition > size || currentPosition < 0)
    {
        TracePrintf(2, "handle_seek: invalid current position (%d)\n", currentPosition);
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
        // calculate the new position and return it
        int new_pos = offset;
        TracePrintf(2, "handle_seek: moved to SEEK_SET position %d\n", new_pos);
        return new_pos;
    }
    if (whence == SEEK_CUR)
    {
        int new_pos = currentPosition + offset;

        // check if the new position is valid
        if (new_pos > size || new_pos < 0)
        {
            TracePrintf(2, "handle_seek: invalid new position (%d) for SEEK_CUR\n", new_pos);
            return ERROR;
        }

        TracePrintf(2, "handle_seek: moved to SEEK_CUR position %d\n", new_pos);
        return new_pos;
    }

    if (whence == SEEK_END)
    {
        int new_pos = size + offset;

        // check if the new position is valid
        if (offset > 0 || new_pos < 0)
        {
            TracePrintf(2, "handle_seek: invalid new position (%d) for SEEK_END\n", new_pos);
            return ERROR;
        }

        TracePrintf(2, "handle_seek: moved to SEEK_END position %d\n", new_pos);
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
    cache_item *currBlockItem = cache_b_queue->q_first;
    while (currBlockItem != NULL)
    {
        // check if this block is dirty (i.e. has been modified)
        if (currBlockItem->dirty)
        {
            // write the block to disk
            WriteSector(currBlockItem->c_item_num, currBlockItem->addr);
            TracePrintf(2, "sync: wrote block %d to disk\n", currBlockItem->c_item_num);
        }

        // move on to the next_inode block in the queue
        currBlockItem = currBlockItem->next_c_item;
    }

    // now, sync all dirty inodes
    cache_item *currInodeItem = cache_i_queue->q_first;
    while (currInodeItem != NULL)
    {
        // check if this inode is dirty (i.e. has been modified)
        if (currInodeItem->dirty)
        {
            // get the block containing this inode
            int inodeNum = currInodeItem->c_item_num;
            int blockNum = (inodeNum / (BLOCKSIZE / INODESIZE)) + 1;

            // copy the inode from the cache to the block
            void *block = get_b(blockNum);
            void *inodeAddrInBlock = (block + (inodeNum - (blockNum - 1) * (BLOCKSIZE / INODESIZE)) * INODESIZE);
            memcpy(inodeAddrInBlock, currInodeItem->addr, sizeof(struct inode));

            // write the block containing the inode to disk
            WriteSector(blockNum, block);
            TracePrintf(2, "sync: wrote inode %d (in block %d) to disk\n", inodeNum, blockNum);
        }

        // move on to the next_inode inode in the queue
        currInodeItem = currInodeItem->next_c_item;
    }

    TracePrintf(1, "finished with sync()\n");
    return 0;
}


/** HASH TABLE HELPER BEGIN */
struct ht *
init_ht(double load_factor, int size)
{
    struct ht *ht;

    assert(load_factor > 0.0);
    ht = malloc(sizeof(struct ht));
    if (ht == NULL)
    {
        return (NULL);
    }

    ht->head = calloc(size, sizeof(ht_map *));
    if (ht->head == NULL)
    {
        free(ht);
        return (NULL);
    }
    ht->size = size;
    ht->num_used = 0;
    ht->load_factor = load_factor;
    return (ht);
}

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
        TracePrintf(1, "error allocating memory for pathname\n");
        return NULL;
    }
    if (CopyFrom(pid, local_pathname, pathname, len) != 0)
    {
        TracePrintf(1, "error copying %d bytes from %p in pid %d to %p locally\n",
                    len, pathname, pid, local_pathname);
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

    cache_item *lruBlockItem = left_dequeue(cache_b_queue);
    int lruBlockNum = lruBlockItem->c_item_num;
    WriteSector(lruBlockNum, lruBlockItem->addr);
    cache_b_size--;
    evict_ht(b_ht, lruBlockNum);

    // Free the memory occupied by the LRU block item
    free(lruBlockItem->addr);
    free(lruBlockItem);
}

// Get LRU block from cache
void *get_b(int b_num)
{
    // find block in hash table
    cache_item *blockItem = (cache_item *)query_ht(b_ht, b_num);

    // if block is found, update position to end of queue
    if (blockItem != NULL)
    {
        right_dequeue(cache_b_queue, blockItem);
        right_enqueue(blockItem, cache_b_queue);
        return blockItem->addr;
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
    cache_item *newItem = malloc(sizeof(cache_item));
    newItem->c_item_num = b_num;
    newItem->addr = block;
    newItem->dirty = false;
    right_enqueue(newItem, cache_b_queue);
    cache_b_size++;
    insert_ht(b_ht, b_num, newItem);

    return block;
}

// evict the LRU inode from the cache
void evict_i()
{
    cache_item *lruInode = left_dequeue(cache_i_queue);
    int lruInodeNum = lruInode->c_item_num;
    cache_i_size--;
    evict_ht(i_ht, lruInodeNum);

    // write the evicted inode back to its block
    int lruBlockNum = (lruInodeNum / (BLOCKSIZE / INODESIZE)) + 1;
    void *lruBlock = get_b(lruBlockNum);
    void *inodeAddrInBlock = (lruBlock + (lruInodeNum - (lruBlockNum - 1) * (BLOCKSIZE / INODESIZE)) * INODESIZE);
    memcpy(inodeAddrInBlock, lruInode->addr, sizeof(struct inode));

    // mark block as dirty
    cache_item *tmpBlockItem = (cache_item *)query_ht(b_ht, lruBlockNum);
    tmpBlockItem->dirty = true;

    // free the memory occupied by the LRU inode item
    free(lruInode->addr);
    free(lruInode);
}

// Get LRU inode from cache
struct inode *get_i(int inodeNum)
{
    cache_item *nodeItem = (cache_item *)query_ht(i_ht, inodeNum);

    // if iniode found, update position to end of queue
    if (nodeItem != NULL)
    {
        right_dequeue(cache_i_queue, nodeItem);
        right_enqueue(nodeItem, cache_i_queue);
        return nodeItem->addr;
    }

    // cache full, evict LRU inode
    if (cache_i_size == INODE_CACHESIZE)
    {
        evict_i();
    }

    // read the requested inode from its block
    int blockNum = (inodeNum / (BLOCKSIZE / INODESIZE)) + 1;
    void *blockAddr = get_b(blockNum);
    struct inode *newInodeAddrInBlock = (struct inode *)(blockAddr + (inodeNum - (blockNum - 1) * (BLOCKSIZE / INODESIZE)) * INODESIZE);

    // copy the inode from its block to a new memory location and create a new cache item for it
    struct inode *inodeCpy = malloc(sizeof(struct inode));
    struct cache_item *inodeItem = malloc(sizeof(struct cache_item));
    memcpy(inodeCpy, newInodeAddrInBlock, sizeof(struct inode));
    inodeItem->addr = inodeCpy;
    inodeItem->c_item_num = inodeNum;

    // add the new inode cache item to the LRU queue and the inode hash table
    right_enqueue(inodeItem, cache_i_queue);
    cache_i_size++;
    insert_ht(i_ht, inodeNum, inodeItem);

    return inodeItem->addr;
}

// get the nth block of an inode. If the block does not exist and allocateIfNeeded is true, allocate a new block.
int get_nth_b(struct inode *inode, int n, bool allocateIfNeeded)
{
    bool isOver = false;

    // check if n exceeds the maximum c_item_num of blocks allowed for an inode
    if (n >= NUM_DIRECT + BLOCKSIZE / (int)sizeof(int))
    {
        return 0;
    }

    // check if the requested block is outside the current file size
    if (n * BLOCKSIZE >= inode->size)
    {
        isOver = true;
    }

    // if the requested block is outside the current file size and allocation is not needed, return 0
    if (isOver && !allocateIfNeeded)
    {
        return 0;
    }

    // if the requested block is within the direct block pointers range
    if (n < NUM_DIRECT)
    {
        // if the requested block is outside the current file size, allocate a new block
        if (isOver)
        {
            inode->direct[n] = get_next_free_bnum();
        }
        return inode->direct[n];
    }

    // if the requested block is within the indirect block pointers range
    // get the indirect block
    int *indirectBlock = get_b(inode->indirect);

    // if the requested block is outside the current file size, allocate a new block
    if (isOver)
    {
        indirectBlock[n - NUM_DIRECT] = get_next_free_bnum();
    }

    // get the block c_item_num from the indirect block
    int blockNum = indirectBlock[n - NUM_DIRECT];

    // return the block c_item_num
    return blockNum;
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

int get_path_i_num(char *path, int inodeStartNumber)
{
    int nextInodeNumber = 0;
    void *block;
    struct inode *inode = get_i(inodeStartNumber);

    if (inode->type == INODE_DIRECTORY)
    {
        int blockNum;
        int offset = get_dir_entry(path, inodeStartNumber, &blockNum, false);
        if (offset != -1)
        {
            block = get_b(blockNum);
            struct dir_entry *dir_entry = (struct dir_entry *)((char *)block + offset);
            // set next_inode inode c_item_num to inode c_item_num of the directory entry
            nextInodeNumber = dir_entry->inum;
        }
    }
    else if (inode->type == INODE_REGULAR || nextInodeNumber == 0)
    {
        return 0;
    }

    char *nextPath = get_next_path_part(path);
    // check if we've reached the end of the path
    if (*nextPath == '\0')
    {
        // get inode pointer using nextInodeNumber
        inode = get_i(nextInodeNumber);
        return nextInodeNumber;
    }

    // recursively call function with next_inode path segment and next_inode inode c_item_num
    return get_path_i_num(nextPath, nextInodeNumber);
}

// grab the next_inode free inode c_item_num from inode freelist
int get_next_inum_free()
{
    if (i_head == NULL)
    {
        return 0;
    }

    int inodeNum = i_head->i_num;
    struct inode *inode = get_i(inodeNum);
    inode->reuse++; // now we have used this inode

    // mark dirty
    cache_item *inodeItem = (cache_item *)query_ht(i_ht, inodeNum);
    inodeItem->dirty = true;

    // Move the first free inode pointer to the next_inode free inode
    i_head = i_head->next_inode;

    return inodeNum;
}

void add_free_i_list(int inodeNum)
{
    free_inode *newHead = malloc(sizeof(free_inode));
    newHead->i_num = inodeNum;
    newHead->next_inode = i_head;
    i_head = newHead;
    free_i_count++;
}

int get_next_free_bnum()
{
    if (b_head == NULL)
    {
        return 0;
    }
    int blockNum = b_head->b_num;
    b_head = b_head->next_inode;
    return blockNum;
}

void add_b_freelist(int blockNum)
{
    free_block *newHead = malloc(sizeof(free_block));
    newHead->b_num = blockNum;
    newHead->next_inode = b_head;
    b_head = newHead;
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
    i_ht = init_ht(1.5, INODE_CACHESIZE + 1);
    b_ht = init_ht(1.5, BLOCK_CACHESIZE + 1);

    // init variables for loop
    int blockNum = 1;
    void *block = get_b(blockNum);

    // init fs header and initialize array of taken blocks
    struct fs_header header = *((struct fs_header *)block);
    bool takenBlocks[header.num_blocks];
    memset(takenBlocks, false, header.num_blocks * sizeof(bool));
    takenBlocks[0] = true;

    int inodeNum = ROOTINODE;
    // loop through all inodes
    while (inodeNum < header.num_inodes)
    {
        // loop through inodes in current block
        while (inodeNum < (BLOCKSIZE / INODESIZE) * blockNum)
        {
            struct inode *inode = get_i(inodeNum);
            if (inode->type == INODE_FREE)
            {
                // if inode is free, create free_inode and add it to freelist
                free_inode *newHead = malloc(sizeof(free_inode));
                newHead->i_num = inodeNum;
                newHead->next_inode = i_head;
                i_head = newHead;
                free_i_count++;
            }
            else
            {
                // if inode is not free, mark the blocks it uses as taken
                int i = 0;
                int blockNum;
                while (1)
                {
                    // loop through all direct and indirect block pointers in inode
                    if (i >= NUM_DIRECT + BLOCKSIZE / (int)sizeof(int))
                    {
                        break;
                    }
                    if (i < NUM_DIRECT)
                    {
                        blockNum = inode->direct[i];
                    }
                    else
                    {
                        int *indirectBlock = get_b(inode->indirect);
                        blockNum = indirectBlock[i - NUM_DIRECT];
                    }
                    // mark block as taken
                    if (blockNum == 0)
                    {
                        break;
                    }
                    takenBlocks[blockNum] = true;
                    i++;
                }
            }
            inodeNum++;
        }
        blockNum++;
        block = get_b(blockNum);
    }

    TracePrintf(1, "initialized free inode list with %d free inodes\n", free_i_count);

    // loop through all blocks and add free blocks to list
    int i;
    for (i = 0; i < header.num_blocks; i++)
    {
        if (!takenBlocks[i])
        {
            free_block *newHead = malloc(sizeof(free_block));
            newHead->b_num = i;
            newHead->next_inode = b_head;
            b_head = newHead;
            free_b_count++;
        }
    }
    TracePrintf(1, "initialized free block list with %d free blocks\n", free_b_count);
}

// This function searches for a directory entry in a given inode for the specified pathname.
// If createIfNeeded is true and the directory entry is not found, it creates a new directory entry.

int get_dir_entry(char *pathname, int inodeStartNumber, int *blockNumPtr, bool createIfNeeded)
{
    // init variables for loop
    int freeEntryOffset = -1;
    int freeEntryBlockNum = 0;
    void *currentBlock;
    struct dir_entry *currentEntry;
    struct inode *inode = get_i(inodeStartNumber);
    int i = 0;
    int blockNum = get_nth_b(inode, i, false);
    int currBlockNum = 0;
    int totalSize = sizeof(struct dir_entry);
    bool isFound = false;

    // loop through all blocks in inode until the directory entry is found or all blocks have been searched
    while (blockNum != 0 && !isFound)
    {
        currentBlock = get_b(blockNum);
        currentEntry = (struct dir_entry *)currentBlock;
        while (totalSize <= inode->size && ((char *)currentEntry < ((char *)currentBlock + BLOCKSIZE)))
        {
            // if a free directory entry is found, remember its offset and block c_item_num
            if (freeEntryOffset == -1 && currentEntry->inum == 0)
            {
                freeEntryBlockNum = blockNum;
                freeEntryOffset = (int)((char *)currentEntry - (char *)currentBlock);
            }

            // compare the directory entry name with specified pathname
            bool isEqual = true;
            int j = 0;
            while (j < DIRNAMELEN)
            {
                if ((pathname[j] == '/' || pathname[j] == '\0') && currentEntry->name[j] == '\0')
                {
                    isEqual = true;
                    break;
                }
                if (pathname[j] != currentEntry->name[j])
                {
                    isEqual = false;
                    break;
                }
                j++;
            }

            // if the names are equal, we've found the directory entry
            if (isEqual)
            {
                isFound = true;
                break;
            }

            // increment the current entry and total size
            currentEntry = (struct dir_entry *)((char *)currentEntry + sizeof(struct dir_entry));
            totalSize += sizeof(struct dir_entry);
        }
        if (isFound)
        {
            break;
        }
        currBlockNum = blockNum;
        blockNum = get_nth_b(inode, ++i, false);
    }

    // set the block c_item_num pointer to the current block c_item_num
    *blockNumPtr = blockNum;

    // if the directory entry was found, return its offset
    if (isFound)
    {
        int offset = (int)((char *)currentEntry - (char *)currentBlock);
        return offset;
    }

    // if we need to create the directory entry and there's free directory entry, use it
    if (createIfNeeded)
    {
        if (freeEntryBlockNum != 0)
        {
            *blockNumPtr = freeEntryBlockNum;
            return freeEntryOffset;
        }

        // if there's no free directory entry, create new block and directory entry
        if (inode->size % BLOCKSIZE == 0)
        {
            blockNum = get_nth_b(inode, i, true);
            currentBlock = get_b(blockNum);
            inode->size += sizeof(struct dir_entry);
            struct dir_entry *newEntry = (struct dir_entry *)currentBlock;
            newEntry->inum = 0;

            // mark the block and inode dirty in the cache
            cache_item *blockItem = (cache_item *)query_ht(b_ht, blockNum);
            blockItem->dirty = true;

            cache_item *inodeItem = (cache_item *)query_ht(i_ht, inodeStartNumber);
            inodeItem->dirty = true;

            *blockNumPtr = blockNum;
            return 0;
        }

        // if the current block still has space, add a new directory entry to it
        inode->size += sizeof(struct dir_entry);
        cache_item *inodeItem = (cache_item *)query_ht(i_ht, inodeStartNumber);
        inodeItem->dirty = true;
        currentEntry->inum = 0;

        // mark the current block and previous block dirty in the cache
        cache_item *blockItem = (cache_item *)query_ht(b_ht, currBlockNum);
        blockItem->dirty = true;
        cache_item *tmpBlockItem = (cache_item *)query_ht(b_ht, currBlockNum);
        tmpBlockItem->dirty = true;

        *blockNumPtr = currBlockNum;
        int offset = (int)((char *)currentEntry - (char *)currentBlock);
        return offset;
    }

    // if the directory entry was not found and we're not creating one, return -1
    return -1;
}

// This function takes in a pathname and the inode c_item_num of the current directory.
// It returns the inode c_item_num of the directory that contains the file specified by pathname.
// It also sets the filename pointer to point to the filename in the pathname.

int get_super_dir(char *pathname, int curr_i, char **filenamePtr)
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
    int lastSlashIndex = 0;
    i = 0;
    while ((pathname[i] != '\0') && (i < MAXPATHNAMELEN))
    {
        if (pathname[i] == '/')
        {
            lastSlashIndex = i;
        }
        i++;
    }

    // if there is a slash in the pathname, get the containing directory's inode c_item_num and set the filename pointer
    if (lastSlashIndex != 0)
    {
        char path[lastSlashIndex + 1];
        for (i = 0; i < lastSlashIndex; i++)
        {
            path[i] = pathname[i];
        }
        path[i] = '\0';

        char *filename = pathname + (sizeof(char) * (lastSlashIndex + 1));
        *filenamePtr = filename;
        int dirInodeNum = get_path_i_num(path, curr_i);
        if (dirInodeNum == 0)
        {
            return ERROR;
        }
        return dirInodeNum;
    }
    // if there is no slash in the pathname, set the filename pointer and return the current directory's inode c_item_num
    else
    {
        *filenamePtr = pathname;
        return curr_i;
    }
}
