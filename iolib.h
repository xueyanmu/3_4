#include <stdbool.h>
#include <comp421/iolib.h>

#define YFS_OPEN        0
#define YFS_CREATE      1
#define YFS_READ        2
#define YFS_WRITE       3
#define YFS_SEEK        4
#define YFS_LINK        5
#define YFS_UNLINK      6
#define YFS_MKDIR       7
#define YFS_RMDIR       8
#define YFS_CHDIR       9
#define YFS_STAT        10
#define YFS_SYNC        11
#define YFS_SHUTDOWN    12

struct m_template {
    int num;
    char padding[28];
};

struct m_path {
    int num;
    int cur_i;
    char *pathname;
    int len;
    char padding[12];
};

struct m_file {
    int num;
    int i_num;
    void *buf;
    int size;
    int offset;
    char padding[8];
};

struct m_link {
    int num;
    int cur_i;
    char *old_name;
    char *new_name;
    int old_len;
    int new_len;
};

struct m_seek {
    int num;
    int i_num;
    int cur_pos;
    int offset;
    int whence;
    char padding[12];
};

struct message_stat {
    int num;
    int cur_i;
    char *pathname;
    int len;
    struct Stat *stat_buffer;
};

int Open(char *);
int Close(int);
int Create(char *);
int Read(int, void *, int);
int Write(int, void *, int);
int Seek(int, int, int);
int Link(char *, char *);
int Unlink(char *);
int SymLink(char *, char *);
int ReadLink(char *, char *, int);
int MkDir(char *);
int RmDir(char *);
int ChDir(char *);
int Stat(char *, struct Stat *);
int Sync(void);
int Shutdown(void);

int get_path_len(char *pathname);
int send_path_m(int operation, char *pathname);
int send_file_m(int operation, int i_num, void *buf, int size, int offset);
