/* POSIX functions used by bflibrary sffind.c, missing in libnix.
 * First stage: file search finds names but cannot stat them (fstatat fails),
 * so listings (save games, packet recordings) come out empty.
 * TODO: proper AmigaDOS ExNext-based file search. */
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

char *realpath(const char *path, char *resolved)
{
    if (resolved == NULL)
        return strdup(path);
    strcpy(resolved, path);
    return resolved;
}

int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
    struct dirent *d;

    d = readdir(dirp);
    if (d == NULL) {
        *result = NULL;
        return 0;
    }
    memcpy(entry, d, sizeof(*entry));
    *result = entry;
    return 0;
}

int dirfd(DIR *dirp)
{
    (void)dirp;
    return -1;
}

int fstatat(int fd, const char *path, struct stat *buf, int flag)
{
    (void)fd; (void)path; (void)buf; (void)flag;
    errno = ENOSYS;
    return -1;
}
