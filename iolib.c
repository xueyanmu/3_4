#include <stdlib.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include "iolib.h"

struct open_file {
    int i_num;
    int position;
};
struct open_file * file_table[MAX_OPEN_FILES] = {NULL};
int files_open = 0;
int cur_i = ROOTINODE;

struct open_file * getFile(int fd);


int Open(char *pathname) {
    int fd;
    int i_num = send_path_m(YFS_OPEN, pathname);
    if (i_num == ERROR) {
        TracePrintf(1, "received error from server\n");
        return ERROR;
    }
    for (fd = 0; fd < MAX_OPEN_FILES; fd++) {
        if (file_table[fd] == NULL) {
            file_table[fd] = malloc(sizeof(struct open_file));
            if (file_table[fd] == NULL) {
                TracePrintf(1, "error allocating space for open file\n");
                return ERROR;
            }
            file_table[fd]->i_num = i_num;
            file_table[fd]->position = 0;
            break;
        }
    }
    if (fd == MAX_OPEN_FILES) {
        TracePrintf(1, "file table full\n");
        return ERROR;
    }
    TracePrintf(2, "inode num %d\n", i_num);
    return fd;
}

int Create(char *pathname) {
    int fd;
    int i_num = send_path_m(YFS_CREATE, pathname);
    if (i_num == ERROR) {
        TracePrintf(1, "received error from server\n");
        return ERROR;
    }
    for (fd = 0; fd < MAX_OPEN_FILES; fd++) {
        if (file_table[fd] == NULL) {
            file_table[fd] = malloc(sizeof(struct open_file));
            if (file_table[fd] == NULL) {
                TracePrintf(1, "error allocating space for open file\n");
                return ERROR;
            }
            file_table[fd]->i_num = i_num;
            file_table[fd]->position = 0;
            break;
        }
    }
    if (fd == MAX_OPEN_FILES) {
        TracePrintf(1, "file table full\n");
        return ERROR;
    }
    TracePrintf(2, "inode num %d\n", i_num);
    return fd;
}

int
Close(int fd)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    struct open_file * file = file_table[fd];
    if (file == NULL) {
        return ERROR;
    }
    free(file);
    file_table[fd] = NULL;
    return 0;
}

int
Read(int fd, void *buf, int size)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    struct open_file * file = file_table[fd];
    if (file == NULL) {
        return ERROR;
    }
    int bytes = send_file_m(YFS_READ, file->i_num, buf, size, file->position);
    if (bytes == ERROR) {
        TracePrintf(1, "received error from server\n");
        return ERROR;
    }
    file->position += bytes;
    return bytes;
}

int
Write(int fd, void *buf, int size)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    struct open_file * file = file_table[fd];
    if (file == NULL) {
        return ERROR;
    }
    int bytes = send_file_m(YFS_WRITE, file->i_num, buf, size, file->position);
    if (bytes == ERROR) {
        TracePrintf(1, "received error from server\n");
        return ERROR;
    }
    file->position += bytes;
    return bytes;
}

int Seek(int fd, int offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        return ERROR;
    }
    struct open_file *file = file_table[fd];
    if (file == NULL) {
        return ERROR;
    }

    //
    if (file->i_num <= 0) {
        return ERROR;
    }
    struct m_seek *msg = malloc(sizeof(struct m_seek));
    if (msg == NULL) {
         TracePrintf(1, "ERROR: allocating space in seek\n");
        return ERROR;
    }
    msg->num = YFS_SEEK;
    msg->i_num = file->i_num;
    msg->cur_pos = file->position;
    msg->offset = offset;
    msg->whence = whence;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "ERROR: sending message to server in seek\n");
        free(msg);
        return ERROR;
    }
    int position = msg->num;
    free(msg);

    if (position == ERROR) {
        TracePrintf(1, "ERROR: received from server in seek\n");
        return ERROR;
    }
    //
    file->position = position;
    return position;
} 
int
Link(char *old_name, char *new_name)
{
    int oldlen = get_path_len(old_name);
    if (oldlen == ERROR) {
        return ERROR;
    }
    int newlen = get_path_len(new_name);
    if (newlen == ERROR) {
        return ERROR;
    }
    struct m_link msg = {
        .num = YFS_LINK,
        .cur_i = cur_i,
        .old_name = old_name,
        .new_name = new_name,
        .old_len = oldlen,
        .new_len = newlen
    };
    int code = Send(&msg, -FILE_SERVER);
    if (code != 0) {
        TracePrintf(1, "error sending message to server\n");
        return ERROR;
    }
    return msg.num;
}

int
Unlink(char *pathname)
{
    int code = send_path_m(YFS_UNLINK, pathname);
    if (code == ERROR) {
        TracePrintf(1, "received error from server\n");
    }
    return code;
}

int
MkDir(char *pathname)
{
    int code = send_path_m(YFS_MKDIR, pathname);
    if (code == ERROR) {
        TracePrintf(1, "received error from server\n");
    }
    return code;
}

int
RmDir(char *pathname)
{
    int code = send_path_m(YFS_RMDIR, pathname);
    if (code == ERROR) {
        TracePrintf(1, "received error from server\n");
    }
    return code;
}

int
ChDir(char *pathname)
{
    int i_num = send_path_m(YFS_CHDIR, pathname);
    if (i_num == ERROR) {
        TracePrintf(1, "received error from server\n");
        return ERROR;
    }
    cur_i = i_num;
    return 0;
}

int Stat(char *pathname, struct Stat *stat_buffer) {
    if (stat_buffer == NULL) {
        return ERROR;
    }
    int len = get_path_len(pathname);
    if (len == ERROR) {
        return ERROR;
    }
    struct message_stat *msg = malloc(sizeof(struct message_stat));
    if (msg == NULL) {
        TracePrintf(1, "ERROR: allocating memory for message in Stat()\n");
        return ERROR;
    }
    msg->num = YFS_STAT;
    msg->cur_i = cur_i; // Ensure cur_i is defined
    msg->pathname = pathname;
    msg->len = len;
    msg->stat_buffer = stat_buffer;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "ERROR: in sending message to server in Stat()\n");
        free(msg);
        return ERROR;
    }
    // msg gets overwritten with reply message after return from Send
    int code = msg->num;
    free(msg);

    if (code == ERROR) {
        TracePrintf(1, "ERROR: from server in Stat()\n");
    }
    return code;
}


int
Sync()
{
    struct m_normal * msg = malloc(sizeof(struct m_normal));
    if (msg == NULL) {
        TracePrintf(1, "error allocating space for path message\n");
        return ERROR;
    }
    msg->num = YFS_SYNC;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "error sending message to server\n");
        free(msg);
        return ERROR;
    }
    // msg gets overwritten with reply message after return from Send
    int code = msg->num;
    free(msg);
    if (code == ERROR) {
        TracePrintf(1, "received error from server\n");
    }
    return code;
}

int
Shutdown()
{
    struct m_normal * msg = malloc(sizeof(struct m_normal));
    if (msg == NULL) {
        TracePrintf(1, "error allocating space for path message\n");
        return ERROR;
    }
    msg->num = YFS_SHUTDOWN;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "error sending message to server\n");
        free(msg);
        return ERROR;
    }
    free(msg);
    return 0;
}

/**
 * BEGIN HELPER FUNCTIONS
*/

int
get_path_len(char *pathname)
{
    if (pathname == NULL) {
        return ERROR;
    }
    int i;
    for (i = 0; i < MAXPATHNAMELEN; i++) {
        if (pathname[i] == '\0') {
            break;
        }
    }
    if (i == 0 || i == MAXPATHNAMELEN) {
        TracePrintf(1, "invalid pathname\n");
        return ERROR;
    }
    return i + 1;
}

int
send_path_m(int operation, char *pathname)
{
    int len = get_path_len(pathname);
    if (len == ERROR) {
        return ERROR;
    }
    struct m_path * msg = malloc(sizeof(struct m_path));
    if (msg == NULL) {
        TracePrintf(1, "error allocating space for path message\n");
        return ERROR;
    }
    msg->num = operation;
    msg->cur_i = cur_i;
    msg->pathname = pathname;
    msg->len = len;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "error sending message to server\n");
        free(msg);
        return ERROR;
    }
    // msg gets overwritten with reply message after return from Send
    int code = msg->num;
    free(msg);
    return code;
}

int
send_file_m(int operation, int i_num, void *buf, int size, int offset)
{
    if (size < 0 || buf == NULL) {
        return ERROR;
    }
    struct m_file * msg = malloc(sizeof(struct m_file));
    if (msg == NULL) {
        TracePrintf(1, "error allocating space for file message\n");
        return ERROR;
    }
    msg->num = operation;
    msg->i_num = i_num;
    msg->buf = buf;
    msg->size = size;
    msg->offset = offset;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "error sending message to server\n");
        free(msg);
        return ERROR;
    }
    int code = msg->num;
    free(msg);
    return code;
}