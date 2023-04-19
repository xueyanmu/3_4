#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include <stdlib.h>
#include <comp421/iolib.h>
#include "iolib.h"

struct opened_file {
    int i_num;
    int pos;
};
struct opened_file * opened_ft[MAX_OPEN_FILES] = {NULL};


struct opened_file * getFile(int fd);
int files_open = 0;
int cur_i = ROOTINODE;
int cur_reuse = 0;

int Open(char *pathname) {
    int fd;
    int i_num = send_path_m(YFS_OPEN, pathname);
    if (i_num == ERROR) {
        TracePrintf(1, "ERROR: RECEIVING MSG\n");
        return ERROR;
    }
    for (fd = 0; fd < MAX_OPEN_FILES; fd++) {
        if (opened_ft[fd] == NULL) {
            opened_ft[fd] = malloc(sizeof(struct opened_file));
            if (opened_ft[fd] == NULL) {
                TracePrintf(1, "ERROR: MALLOC\n");
                return ERROR;
            }
            opened_ft[fd]->i_num = i_num;
            opened_ft[fd]->pos = 0;
            break;
        }
    }
    if (fd == MAX_OPEN_FILES) {
        return ERROR;
    }
    return fd;
}

int Create(char *pathname) {
    int fd;
    int i_num = send_path_m(YFS_CREATE, pathname);
    if (i_num == ERROR) {
        TracePrintf(1, "ERROR: RECEIVING MSG\n");
        return ERROR;
    }
    for (fd = 0; fd < MAX_OPEN_FILES; fd++) {
        if (opened_ft[fd] == NULL) {
            opened_ft[fd] = malloc(sizeof(struct opened_file));
            if (opened_ft[fd] == NULL) {
                TracePrintf(1, "ERROR: MALLOC\n");
                return ERROR;
            }
            opened_ft[fd]->i_num = i_num;
            opened_ft[fd]->pos = 0;
            break;
        }
    }
    if (fd == MAX_OPEN_FILES) {
        return ERROR;
    }
    return fd;
}

int
Close(int fd)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    struct opened_file * file = opened_ft[fd];
    if (file == NULL) {
        return ERROR;
    }
    free(file);
    opened_ft[fd] = NULL;
    return 0;
}

int
Read(int fd, void *buf, int size)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    struct opened_file * file = opened_ft[fd];
    if (file == NULL) {
        return ERROR;
    }
    int bytes = send_file_m(YFS_READ, file->i_num, buf, size, file->pos);
    if (bytes == ERROR) {
        TracePrintf(1, "ERROR: RECEIVING MSG\n");
        return ERROR;
    }
    file->pos += bytes;
    return bytes;
}

int
Write(int fd, void *buf, int size)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    struct opened_file * file = opened_ft[fd];
    if (file == NULL) {
        return ERROR;
    }
    int bytes = send_file_m(YFS_WRITE, file->i_num, buf, size, file->pos);
    if (bytes == ERROR) {
        TracePrintf(1, "ERROR: RECEIVING MSG\n");
        return ERROR;
    }
    file->pos += bytes;
    return bytes;
}

int Seek(int fd, int offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR;
    }
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        return ERROR;
    }
    struct opened_file *file = opened_ft[fd];
    if (file == NULL) {
        return ERROR;
    }

    //
    if (file->i_num <= 0) {
        return ERROR;
    }
    struct m_seek *msg = malloc(sizeof(struct m_seek));
    if (msg == NULL) {
         TracePrintf(1, "ERROR: SEEK MALLOC\n");
        return ERROR;
    }
    msg->num = YFS_SEEK;
    msg->i_num = file->i_num;
    msg->cur_pos = file->pos;
    msg->offset = offset;
    msg->whence = whence;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "ERROR: SEEK SEND MSG\n");
        free(msg);
        return ERROR;
    }
    int pos = msg->num;
    free(msg);

    if (pos == ERROR) {
        TracePrintf(1, "ERROR: RECEIVING MSG IN SEEK\n");
        return ERROR;
    }
    //
    file->pos = pos;
    return pos;
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
        TracePrintf(1, "ERROR: SENDING MSG IN LINK\n");
        return ERROR;
    }
    return msg.num;
}

int
Unlink(char *pathname)
{
    int code = send_path_m(YFS_UNLINK, pathname);
    if (code == ERROR) {
        TracePrintf(1, "ERROR: RECEIVING MSG\n");
    }
    return code;
}

int
MkDir(char *pathname)
{
    int code = send_path_m(YFS_MKDIR, pathname);
    if (code == ERROR) {
        TracePrintf(1, "ERROR: RECEIVING MSG\n");
    }
    return code;
}

int
RmDir(char *pathname)
{
    int code = send_path_m(YFS_RMDIR, pathname);
    if (code == ERROR) {
        TracePrintf(1, "ERROR: RECEIVING MSG\n");
    }
    return code;
}

int
ChDir(char *pathname)
{
    int i_num = send_path_m(YFS_CHDIR, pathname);
    if (i_num == ERROR) {
        TracePrintf(1, "ERROR: RECEIVING MSG\n");
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
        TracePrintf(1, "ERROR: MALLOC IN STAT()\n");
        return ERROR;
    }
    msg->num = YFS_STAT;
    msg->cur_i = cur_i; // Ensure cur_i is defined
    msg->pathname = pathname;
    msg->len = len;
    msg->stat_buffer = stat_buffer;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "ERROR: SENDING MSG IN STAT()\n");
        free(msg);
        return ERROR;
    }
    int code = msg->num;
    free(msg);

    if (code == ERROR) {
        TracePrintf(1, "ERROR: RECEIVING MSG IN STAT()\n");
    }
    return code;
}


int
Sync()
{
    struct m_template * msg = malloc(sizeof(struct m_template));
    if (msg == NULL) {
        TracePrintf(1, "ERROR: MALLOC \n");
        return ERROR;
    }
    msg->num = YFS_SYNC;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "ERROR: SENDING MSG\n");
        free(msg);
        return ERROR;
    }
    int code = msg->num;
    free(msg);
    if (code == ERROR) {
        TracePrintf(1, "ERROR: RECEIVING MSG\n");
    }
    return code;
}

int
Shutdown()
{
    struct m_template * msg = malloc(sizeof(struct m_template));
    if (msg == NULL) {
        TracePrintf(1, "ERROR: MALLOC \n");
        return ERROR;
    }
    msg->num = YFS_SHUTDOWN;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "ERROR: SENDING MSG\n");
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
        TracePrintf(1, "ERROR: INVALID PATH\n");
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
        TracePrintf(1, "ERROR: MALLOC \n");
        return ERROR;
    }
    msg->num = operation;
    msg->cur_i = cur_i;
    msg->pathname = pathname;
    msg->len = len;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "ERROR: SENDING MSG\n");
        free(msg);
        return ERROR;
    }
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
        TracePrintf(1, "ERROR: MALLOC\n");
        return ERROR;
    }
    msg->num = operation;
    msg->i_num = i_num;
    msg->buf = buf;
    msg->size = size;
    msg->offset = offset;
    if (Send(msg, -FILE_SERVER) != 0) {
        TracePrintf(1, "ERROR: SENDING MSG\n");
        free(msg);
        return ERROR;
    }
    int code = msg->num;
    free(msg);
    return code;
}