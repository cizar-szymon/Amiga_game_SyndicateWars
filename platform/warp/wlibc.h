/* wlibc.h - Warp ARM build: the POSIX part of the C library the game uses
 * (files, directories, getopt, time), implemented in wlibc.c over the link to
 * the 68k host (wlink.c), next to Arm Compiler's microlib.  Forced in front of
 * every source by platform/warp/config.h; the POSIX headers in
 * platform/warp/include only include this file. */
#ifndef WLIBC_H
#define WLIBC_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* errno values microlib's errno.h lacks */
#ifndef ENOENT
#define ENOENT 2
#endif
#ifndef EIO
#define EIO 5
#endif
#ifndef EBADF
#define EBADF 9
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif
#ifndef EACCES
#define EACCES 13
#endif
#ifndef EEXIST
#define EEXIST 17
#endif
#ifndef ENOTDIR
#define ENOTDIR 20
#endif
#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EMFILE
#define EMFILE 24
#endif
#ifndef ENAMETOOLONG
#define ENAMETOOLONG 36
#endif
#ifndef ENOSYS
#define ENOSYS 38
#endif

typedef long off_t;
typedef long ssize_t;
typedef unsigned int mode_t;
typedef int pid_t;
typedef long useconds_t;

/* files: fcntl.h / unistd.h */
#define O_RDONLY  0x0000
#define O_WRONLY  0x0001
#define O_RDWR    0x0002
#define O_ACCMODE 0x0003
#define O_CREAT   0x0100
#define O_TRUNC   0x0200
#define O_APPEND  0x0400
#define O_EXCL    0x0800
#ifndef O_BINARY
#define O_BINARY  0
#endif
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

int open(const char *path, int flags, ...);
int close(int fd);
ssize_t read(int fd, void *buf, size_t len);
ssize_t write(int fd, const void *buf, size_t len);
off_t lseek(int fd, off_t off, int whence);
int access(const char *path, int mode);
int unlink(const char *path);
int rmdir(const char *path);
char *getcwd(char *buf, size_t size);
int chdir(const char *path);
int usleep(useconds_t us);
unsigned int sleep(unsigned int s);
int isatty(int fd);

/* sys/stat.h */
#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXU 0700
#define S_IRGRP 040
#define S_IWGRP 020
#define S_IXGRP 010
#define S_IRWXG 070
#define S_IROTH 04
#define S_IWOTH 02
#define S_IXOTH 01
#define S_IRWXO 07

struct stat {
    mode_t st_mode;
    off_t st_size;
    time_t st_mtime;
    time_t st_atime;
    time_t st_ctime;
};
int stat(const char *path, struct stat *st);
int fstat(int fd, struct stat *st);
int mkdir(const char *path, mode_t mode);
int fstatat(int fd, const char *path, struct stat *buf, int flag);
#define AT_FDCWD (-100)

/* dirent.h: no directory listing over the link yet (empty listings) */
struct dirent {
    char d_name[256];
};
typedef struct WDir DIR;
DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
int closedir(DIR *d);
int dirfd(DIR *d);
char *realpath(const char *path, char *resolved);

/* strings.h */
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);
char *strdup(const char *s);
char *strndup(const char *s, size_t n);
size_t strnlen(const char *s, size_t n);
char *strtok_r(char *s, const char *delim, char **save);
char *stpcpy(char *d, const char *s);

/* libgen.h */
char *basename(char *path);
char *dirname(char *path);

/* getopt.h */
struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};
#define no_argument 0
#define required_argument 1
#define optional_argument 2
extern char *optarg;
extern int optind, opterr, optopt;
int getopt(int argc, char *const argv[], const char *optstring);
int getopt_long(int argc, char *const argv[], const char *optstring,
  const struct option *longopts, int *longindex);

/* microlib's printf family (one object with vsnprintf) writes through
   fputc; these go to the link instead */
int w_printf(const char *fmt, ...);
int w_fprintf(FILE *f, const char *fmt, ...);
int w_vprintf(const char *fmt, va_list ap);
int w_vfprintf(FILE *f, const char *fmt, va_list ap);
#define printf w_printf
#define fprintf w_fprintf
#define vprintf w_vprintf
#define vfprintf w_vfprintf

/* the extension's own: milliseconds since the start, and the 68k log */
uint32_t w_millis(void);
uint32_t w_cycles(void);
void w_log(const char *text);

#ifdef __cplusplus
}
#endif

#endif
