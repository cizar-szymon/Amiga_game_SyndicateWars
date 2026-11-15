/* wlibc.c - Warp ARM build: heap, files, stdio streams, getopt and the rest
 * of the C library the game needs beyond Arm Compiler's microlib.
 * Files live on the 68k: every operation is a request over the link
 * (wlink.c, swlink.h); reads come in through the transfer buffer and are
 * cached per open file (64 KB windows), as the game reads in small pieces. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include "wlibc.h"
#include "wlink.h"

/******************************************************************************/
/* heap: first fit over the arena, blocks in address order, 16-byte aligned */

typedef struct HBlock {
    uint32_t size;      /* bytes of the block, header included */
    uint32_t used;
    uint32_t pad[2];
} HBlock;

/* Two pools: the arena of the extension, and a second one in the render
 * area (w_heap_add) used when the first is full; free() needs no pool, the
 * block header is enough. */
typedef struct {
    uint8_t *base, *end;
    HBlock *rover;
} HPool;

static HPool pools[2];
static int n_pools;
uint32_t w_heap_used, w_heap_peak, w_heap2_used;

static void pool_init(HPool *pl, void *base, uint32_t size)
{
    uintptr_t b = ((uintptr_t)base + 15) & ~(uintptr_t)15;
    size -= (uint32_t)(b - (uintptr_t)base);
    size &= ~15u;
    pl->base = (uint8_t *)b;
    pl->end = pl->base + size;
    pl->rover = (HBlock *)pl->base;
    pl->rover->size = size;
    pl->rover->used = 0;
}

void w_heap_init(void *base, uint32_t size)
{
    pool_init(&pools[0], base, size);
    n_pools = 1;
    w_heap_used = w_heap_peak = w_heap2_used = 0;
}

void w_heap_add(void *base, uint32_t size)
{
    if ((n_pools >= 2) || (size < 65536))
        return;
    pool_init(&pools[n_pools], base, size);
    n_pools++;
}

static HBlock *hnext(HBlock *b)
{
    return (HBlock *)((uint8_t *)b + b->size);
}

static void *pool_alloc(HPool *pl, uint32_t need)
{
    HBlock *b, *start;
    int wrapped = 0;

    b = start = pl->rover;
    for (;;) {
        /* a broken block header would loop forever: report it */
        if ((b->size < sizeof(HBlock)) || ((b->size & 15) != 0) ||
          ((uint8_t *)b + b->size > pl->end)) {
            char m[160];
            snprintf(m, sizeof(m), "wlibc: heap pool %p..%p broken at %p: size 0x%lx used 0x%lx\n",
              (void *)pl->base, (void *)pl->end, (void *)b, (unsigned long)b->size,
              (unsigned long)b->used);
            wl_log(m);
            return NULL;
        }
        if (!b->used) {
            /* merge the free blocks after this one */
            HBlock *nx = hnext(b);
            while ((uint8_t *)nx < pl->end && !nx->used) {
                if (pl->rover == nx) pl->rover = b;
                b->size += nx->size;
                nx = hnext(b);
            }
            if (b->size >= need) {
                if (b->size - need >= 64) {
                    HBlock *rest = (HBlock *)((uint8_t *)b + need);
                    rest->size = b->size - need;
                    rest->used = 0;
                    b->size = need;
                }
                b->used = 1;
                pl->rover = b;
                return b + 1;
            }
        }
        b = hnext(b);
        if ((uint8_t *)b >= pl->end) {
            /* One round only: the start block may have been merged into the
             * one before it, and then b never comes back to it (that looped
             * forever when the heap was full, entering a mission from the
             * menu). */
            if (wrapped)
                break;
            b = (HBlock *)pl->base;
            wrapped = 1;
        }
        if (wrapped && ((uint8_t *)b >= (uint8_t *)start))
            break;
    }
    return NULL;
}

void *malloc(size_t n)
{
    uint32_t need = (uint32_t)((n + sizeof(HBlock) + 15) & ~15u);
    void *p = NULL;
    int i;

    if (n_pools == 0 || n > 0x7FFFFFF0u)
        return NULL;
    for (i = 0; i < n_pools; i++) {
        p = pool_alloc(&pools[i], need);
        if (p != NULL)
            break;
    }
    if (p != NULL) {
        HBlock *b = (HBlock *)p - 1;
        w_heap_used += b->size;
        if (i > 0)
            w_heap2_used += b->size;
        if (w_heap_used > w_heap_peak) w_heap_peak = w_heap_used;
        if (wl_trace_mode && n > 65536) {
            char m[96];
            snprintf(m, sizeof(m), "malloc %lu -> %p\n", (unsigned long)n, p);
            wl_log(m);
        }
        return p;
    }
    {
        char m[96];
        snprintf(m, sizeof(m), "wlibc: malloc(%lu) failed, heap used %lu (second pool %lu)\n",
          (unsigned long)n, (unsigned long)w_heap_used, (unsigned long)w_heap2_used);
        wl_log(m);
    }
    return NULL;
}

void free(void *p)
{
    HBlock *b;
    if (p == NULL)
        return;
    b = (HBlock *)p - 1;
    if (!b->used)
        return;
    b->used = 0;
    w_heap_used -= b->size;
    if ((n_pools > 1) && ((uint8_t *)b >= pools[1].base) && ((uint8_t *)b < pools[1].end))
        w_heap2_used -= b->size;
}

void *calloc(size_t n, size_t size)
{
    void *p = malloc(n * size);
    if (p)
        memset(p, 0, n * size);
    return p;
}

void *realloc(void *p, size_t n)
{
    HBlock *b;
    void *q;
    size_t old;

    if (p == NULL)
        return malloc(n);
    if (n == 0) {
        free(p);
        return NULL;
    }
    b = (HBlock *)p - 1;
    old = b->size - sizeof(HBlock);
    if (old >= n)
        return p;
    q = malloc(n);
    if (q) {
        memcpy(q, p, old);
        free(p);
    }
    return q;
}

/******************************************************************************/
/* files */

#define MAX_FD 32
#define CACHE_BYTES (64 * 1024)

typedef struct {
    int used;
    int32_t handle;     /* the 68k's */
    int flags;
    uint32_t pos, size;
    uint8_t *cache;
    uint32_t cache_off, cache_len;
} WFile;

static WFile fds[MAX_FD];
static uint8_t *xfer;   /* SWL_BUF_XFER, in the render area */

uint8_t *w_xfer_buf(void)
{
    return xfer;
}

void w_files_init(uint8_t *xfer_buf)
{
    memset(fds, 0, sizeof(fds));
    fds[0].used = fds[1].used = fds[2].used = 1;
    xfer = xfer_buf;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

int open(const char *path, int flags, ...)
{
    uint8_t a[4];
    int32_t size = 0;
    int h, fd;

    for (fd = 3; fd < MAX_FD; fd++)
        if (!fds[fd].used)
            break;
    if (fd >= MAX_FD) {
        errno = EMFILE;
        return -1;
    }
    if (wl_trace_mode) {
        char m[300];
        snprintf(m, sizeof(m), "open %s %x\n", path, flags);
        wl_log(m);
    }
    put32(a, (uint32_t)flags);
    h = wl_request(SWL_FOPEN, a, 4, path, (unsigned)strlen(path) + 1, &size);
    if (h < 0) {
        errno = ENOENT;
        return -1;
    }
    memset(&fds[fd], 0, sizeof(fds[fd]));
    fds[fd].used = 1;
    fds[fd].handle = h;
    fds[fd].flags = flags;
    fds[fd].size = (uint32_t)size;
    return fd;
}

int close(int fd)
{
    uint8_t a[4];
    if (fd < 3 || fd >= MAX_FD || !fds[fd].used) {
        errno = EBADF;
        return -1;
    }
    put32(a, (uint32_t)fds[fd].handle);
    wl_request(SWL_FCLOSE, a, 4, NULL, 0, NULL);
    if (fds[fd].cache)
        free(fds[fd].cache);
    fds[fd].used = 0;
    return 0;
}

/* read len bytes at off from the 68k into dst (len <= SWL_XFER_BYTES) */
static int32_t fetch(WFile *f, uint32_t off, void *dst, uint32_t len)
{
    uint8_t a[12];
    int32_t n;

    put32(a, (uint32_t)f->handle);
    put32(a + 4, off);
    put32(a + 8, len);
    n = wl_request(SWL_FREAD, a, 12, NULL, 0, NULL);
    if (n > 0) {
        wl_env->cacheInvalidate(xfer, ((uint32_t)n + 31) & ~31u);
        memcpy(dst, xfer, (uint32_t)n);
    }
    return n;
}

ssize_t read(int fd, void *buf, size_t len)
{
    WFile *f;
    uint8_t *d = (uint8_t *)buf;
    size_t done = 0;

    if (fd < 3 || fd >= MAX_FD || !fds[fd].used) {
        errno = EBADF;
        return -1;
    }
    f = &fds[fd];
    while (done < len) {
        uint32_t want = (uint32_t)(len - done);
        if (f->cache && f->pos >= f->cache_off && f->pos < f->cache_off + f->cache_len) {
            uint32_t k = f->cache_off + f->cache_len - f->pos;
            if (k > want) k = want;
            memcpy(d + done, f->cache + (f->pos - f->cache_off), k);
            f->pos += k;
            done += k;
            continue;
        }
        if (f->pos >= f->size)
            break;
        if (want >= CACHE_BYTES) {
            int32_t n;
            if (want > SWL_XFER_BYTES) want = SWL_XFER_BYTES;
            n = fetch(f, f->pos, d + done, want);
            if (n <= 0) break;
            f->pos += (uint32_t)n;
            done += (uint32_t)n;
            continue;
        }
        if (f->cache == NULL) {
            f->cache = (uint8_t *)malloc(CACHE_BYTES);
            if (f->cache == NULL) return -1;
        }
        {
            int32_t n = fetch(f, f->pos, f->cache, CACHE_BYTES);
            if (n <= 0) break;
            f->cache_off = f->pos;
            f->cache_len = (uint32_t)n;
        }
    }
    return (ssize_t)done;
}

ssize_t write(int fd, const void *buf, size_t len)
{
    WFile *f;
    const uint8_t *s = (const uint8_t *)buf;
    size_t done = 0;

    if (fd == 1 || fd == 2) {
        char line[SWL_REC_MAX + 1];
        size_t k = len < SWL_REC_MAX ? len : SWL_REC_MAX;
        memcpy(line, buf, k);
        line[k] = 0;
        wl_log(line);
        return (ssize_t)len;
    }
    if (fd < 3 || fd >= MAX_FD || !fds[fd].used) {
        errno = EBADF;
        return -1;
    }
    f = &fds[fd];
    f->cache_len = 0;
    if (f->flags & O_APPEND)
        f->pos = f->size;
    while (done < len) {
        uint8_t a[8];
        uint32_t k = (uint32_t)(len - done);
        int32_t n;
        if (k > SWL_REC_MAX - 8) k = SWL_REC_MAX - 8;
        put32(a, (uint32_t)f->handle);
        put32(a + 4, f->pos);
        n = wl_request(SWL_FWRITE, a, 8, s + done, k, NULL);
        if (n <= 0) break;
        f->pos += (uint32_t)n;
        done += (uint32_t)n;
        if (f->pos > f->size) f->size = f->pos;
    }
    return (ssize_t)done;
}

off_t lseek(int fd, off_t off, int whence)
{
    WFile *f;
    long np;

    if (fd < 3 || fd >= MAX_FD || !fds[fd].used) {
        errno = EBADF;
        return -1;
    }
    f = &fds[fd];
    switch (whence) {
    case SEEK_SET: np = off; break;
    case SEEK_CUR: np = (long)f->pos + off; break;
    case SEEK_END: np = (long)f->size + off; break;
    default: errno = EINVAL; return -1;
    }
    if (np < 0) {
        errno = EINVAL;
        return -1;
    }
    f->pos = (uint32_t)np;
    return np;
}

int stat(const char *path, struct stat *st)
{
    int32_t isdir = 0;
    int r = wl_request(SWL_FSTAT, path, (unsigned)strlen(path) + 1, NULL, 0, &isdir);
    if (r < 0) {
        errno = ENOENT;
        return -1;
    }
    memset(st, 0, sizeof(*st));
    st->st_size = r;
    st->st_mode = isdir ? (S_IFDIR | 0755) : (S_IFREG | 0644);
    return 0;
}

int fstat(int fd, struct stat *st)
{
    if (fd < 3 || fd >= MAX_FD || !fds[fd].used) {
        errno = EBADF;
        return -1;
    }
    memset(st, 0, sizeof(*st));
    st->st_size = (off_t)fds[fd].size;
    st->st_mode = S_IFREG | 0644;
    return 0;
}

int fstatat(int fd, const char *path, struct stat *buf, int flag)
{
    (void)fd; (void)flag;
    return stat(path, buf);
}

int access(const char *path, int mode)
{
    struct stat st;
    (void)mode;
    return stat(path, &st);
}

int remove(const char *path)
{
    return wl_request(SWL_FREMOVE, path, (unsigned)strlen(path) + 1, NULL, 0, NULL) < 0 ? -1 : 0;
}

int unlink(const char *path) { return remove(path); }
int rmdir(const char *path) { return remove(path); }

int rename(const char *a, const char *b)
{
    return wl_request(SWL_FRENAME, a, (unsigned)strlen(a) + 1, b, (unsigned)strlen(b) + 1, NULL) < 0
      ? -1 : 0;
}

int mkdir(const char *path, mode_t mode)
{
    (void)mode;
    return wl_request(SWL_MKDIR, path, (unsigned)strlen(path) + 1, NULL, 0, NULL) < 0 ? -1 : 0;
}

char *getcwd(char *buf, size_t size)
{
    if (buf == NULL || size < 9)
        return NULL;
    strcpy(buf, "PROGDIR:");
    return buf;
}

int chdir(const char *path) { (void)path; return 0; }
int fsync(int fd) { (void)fd; return 0; }
int isatty(int fd) { (void)fd; return 0; }

/* directories: no listing over the link yet */
DIR *opendir(const char *path) { (void)path; errno = ENOENT; return NULL; }
struct dirent *readdir(DIR *d) { (void)d; return NULL; }
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
    (void)dirp; (void)entry;
    *result = NULL;
    return 0;
}
int closedir(DIR *d) { (void)d; return 0; }
int dirfd(DIR *d) { (void)d; return -1; }

char *realpath(const char *path, char *resolved)
{
    if (resolved == NULL)
        return strdup(path);
    strcpy(resolved, path);
    return resolved;
}

/******************************************************************************/
/* stdio streams over the descriptors */

struct __FILE {
    int fd;
    int eof, err;
    int ungot;
};

FILE __stdin = { 0, 0, 0, -1 }, __stdout = { 1, 0, 0, -1 }, __stderr = { 2, 0, 0, -1 };
static FILE streams[16];

FILE *fopen(const char *path, const char *mode)
{
    int flags, fd, i;
    int plus = strchr(mode, '+') != NULL;

    switch (mode[0]) {
    case 'r': flags = plus ? O_RDWR : O_RDONLY; break;
    case 'w': flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC; break;
    case 'a': flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND; break;
    default: errno = EINVAL; return NULL;
    }
    for (i = 0; i < 16; i++)
        if (streams[i].fd == 0)
            break;
    if (i >= 16)
        return NULL;
    fd = open(path, flags);
    if (fd < 0)
        return NULL;
    streams[i].fd = fd;
    streams[i].eof = streams[i].err = 0;
    streams[i].ungot = -1;
    return &streams[i];
}

int fclose(FILE *f)
{
    int r;
    if (f == NULL || f->fd < 3)
        return 0;
    r = close(f->fd);
    f->fd = 0;
    return r;
}

size_t fread(void *buf, size_t size, size_t n, FILE *f)
{
    size_t total = size * n;
    ssize_t r;
    uint8_t *d = (uint8_t *)buf;
    if (total == 0)
        return 0;
    if (f->ungot >= 0) {
        *d++ = (uint8_t)f->ungot;
        f->ungot = -1;
        total--;
        r = total ? read(f->fd, d, total) : 0;
        if (r < 0) r = 0;
        r += 1;
        total++;
    } else {
        r = read(f->fd, d, total);
    }
    if (r < 0) { f->err = 1; return 0; }
    if ((size_t)r < total) f->eof = 1;
    return (size_t)r / size;
}

size_t fwrite(const void *buf, size_t size, size_t n, FILE *f)
{
    ssize_t r;
    if (size * n == 0)
        return 0;
    r = write(f->fd, buf, size * n);
    if (r < 0) { f->err = 1; return 0; }
    return (size_t)r / size;
}

int fseek(FILE *f, long off, int whence)
{
    f->ungot = -1;
    f->eof = 0;
    return lseek(f->fd, off, whence) < 0 ? -1 : 0;
}

long ftell(FILE *f)
{
    if (f->fd < 3) return 0;
    return (long)fds[f->fd].pos - (f->ungot >= 0 ? 1 : 0);
}

void rewind(FILE *f) { fseek(f, 0, SEEK_SET); f->err = 0; }
int fflush(FILE *f) { (void)f; return 0; }
int feof(FILE *f) { return f->eof; }
int ferror(FILE *f) { return f->err; }
void clearerr(FILE *f) { f->eof = f->err = 0; }
int setvbuf(FILE *f, char *buf, int mode, size_t size) { (void)f; (void)buf; (void)mode; (void)size; return 0; }
void setbuf(FILE *f, char *buf) { (void)f; (void)buf; }
int fileno(FILE *f) { return f->fd; }

int fgetc(FILE *f)
{
    uint8_t c;
    if (f->ungot >= 0) {
        int r = f->ungot;
        f->ungot = -1;
        return r;
    }
    if (read(f->fd, &c, 1) != 1) {
        f->eof = 1;
        return EOF;
    }
    return c;
}

int getc(FILE *f) { return fgetc(f); }

int ungetc(int c, FILE *f)
{
    if (c == EOF) return EOF;
    f->ungot = c & 0xFF;
    f->eof = 0;
    return c;
}

char *fgets(char *s, int n, FILE *f)
{
    int i = 0;
    while (i < n - 1) {
        int c = fgetc(f);
        if (c == EOF) break;
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    if (i == 0) return NULL;
    s[i] = 0;
    return s;
}

int fputc(int c, FILE *f)
{
    uint8_t b = (uint8_t)c;
    return fwrite(&b, 1, 1, f) == 1 ? c : EOF;
}

int putc(int c, FILE *f) { return fputc(c, f); }
#undef putchar
int putchar(int c) { return fputc(c, stdout); }

int fputs(const char *s, FILE *f)
{
    size_t n = strlen(s);
    return fwrite(s, 1, n, f) == n ? 0 : EOF;
}

int puts(const char *s)
{
    char line[512];
    snprintf(line, sizeof(line), "%s\n", s);
    wl_log(line);
    return 0;
}

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    static char buf[2048];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n < 0) return n;
    if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;
    return (int)fwrite(buf, 1, (size_t)n, f);
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

int vprintf(const char *fmt, va_list ap)
{
    return vfprintf(stdout, fmt, ap);
}

int printf(const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return n;
}

/******************************************************************************/
/* strings */

int strcasecmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb || ca == 0) return ca - cb;
    }
}

int strncasecmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb || ca == 0) return ca - cb;
    }
    return 0;
}

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

char *strndup(const char *s, size_t n)
{
    size_t k = strnlen(s, n);
    char *d = (char *)malloc(k + 1);
    if (d) { memcpy(d, s, k); d[k] = 0; }
    return d;
}

size_t strnlen(const char *s, size_t n)
{
    size_t k = 0;
    while (k < n && s[k]) k++;
    return k;
}

char *strtok_r(char *s, const char *delim, char **save)
{
    char *e;
    if (s == NULL) s = *save;
    s += strspn(s, delim);
    if (*s == 0) { *save = s; return NULL; }
    e = s + strcspn(s, delim);
    if (*e) *e++ = 0;
    *save = e;
    return s;
}

char *stpcpy(char *d, const char *s)
{
    while ((*d = *s++) != 0) d++;
    return d;
}

char *basename(char *path)
{
    char *p = strrchr(path, '/');
    char *q = strrchr(path, ':');
    if (q > p) p = q;
    return p ? p + 1 : path;
}

char *dirname(char *path)
{
    char *p = strrchr(path, '/');
    if (p == NULL) {
        p = strrchr(path, ':');
        if (p == NULL) return (char *)".";
        p[1] = 0;
        return path;
    }
    *p = 0;
    return path;
}

/******************************************************************************/
/* getopt */

char *optarg;
int optind = 1, opterr = 1, optopt;
static int optpos;

int getopt_long(int argc, char *const argv[], const char *optstring,
  const struct option *longopts, int *longindex)
{
    const char *arg, *o;

    optarg = NULL;
    if (optind >= argc)
        return -1;
    arg = argv[optind];
    if (optpos == 0) {
        if (arg[0] != '-' || arg[1] == 0)
            return -1;
        if (arg[1] == '-') {
            const char *name = arg + 2;
            size_t nl = strcspn(name, "=");
            int i;
            optind++;
            if (*name == 0)
                return -1;
            for (i = 0; longopts && longopts[i].name; i++) {
                if (strlen(longopts[i].name) == nl && strncmp(longopts[i].name, name, nl) == 0) {
                    if (longindex) *longindex = i;
                    if (longopts[i].has_arg) {
                        if (name[nl] == '=') optarg = (char *)name + nl + 1;
                        else if (longopts[i].has_arg == required_argument && optind < argc)
                            optarg = argv[optind++];
                    }
                    if (longopts[i].flag) {
                        *longopts[i].flag = longopts[i].val;
                        return 0;
                    }
                    return longopts[i].val;
                }
            }
            return '?';
        }
        optpos = 1;
    }
    optopt = arg[optpos];
    o = strchr(optstring, optopt);
    optpos++;
    if (o == NULL || optopt == ':') {
        if (arg[optpos] == 0) { optind++; optpos = 0; }
        return '?';
    }
    if (o[1] == ':') {
        if (arg[optpos]) optarg = (char *)arg + optpos;
        else if (optind + 1 < argc) optarg = argv[++optind];
        optind++;
        optpos = 0;
    } else if (arg[optpos] == 0) {
        optind++;
        optpos = 0;
    }
    return optopt;
}

int getopt(int argc, char *const argv[], const char *optstring)
{
    return getopt_long(argc, argv, optstring, NULL, NULL);
}

/******************************************************************************/
/* time */

static uint32_t cyc_last, cyc_rem;
static uint32_t ms_total;

/** Milliseconds since the start, from the core cycle counter. */
uint32_t w_millis(void)
{
    uint32_t now = wl_env->cycles();
    uint32_t per_ms = wl_env->cpuHz / 1000u;
    cyc_rem += now - cyc_last;
    cyc_last = now;
    if (cyc_rem >= per_ms) {
        uint32_t k = cyc_rem / per_ms;
        ms_total += k;
        cyc_rem -= k * per_ms;
    }
    return ms_total;
}

uint32_t w_cycles(void)
{
    return wl_env->cycles();
}

void w_time_init(void)
{
    cyc_last = wl_env->cycles();
    cyc_rem = 0;
    ms_total = 0;
}

time_t w_start_time;

clock_t clock(void)
{
    /* milliseconds, as the game's update_tick_time() expects */
    return (clock_t)w_millis();
}

time_t time(time_t *t)
{
    time_t v = w_start_time + (time_t)(w_millis() / 1000u);
    if (t) *t = v;
    return v;
}

int usleep(useconds_t us)
{
    uint32_t end = w_millis() + (uint32_t)(us / 1000);
    while ((int32_t)(w_millis() - end) < 0)
        wl_poll();
    return 0;
}

unsigned int sleep(unsigned int s)
{
    usleep((useconds_t)s * 1000000);
    return 0;
}

void w_log(const char *text)
{
    wl_log(text);
}

/******************************************************************************/
/* exit and asserts */

#include <setjmp.h>
extern jmp_buf w_exit_jump;
extern int w_exit_code;

void exit(int code)
{
    w_exit_code = code;
    longjmp(w_exit_jump, 1);
}

void abort(void)
{
    wl_log("abort()\n");
    exit(3);
}

void __aeabi_assert(const char *expr, const char *file, int line)
{
    char m[256];
    snprintf(m, sizeof(m), "assert failed: %s, %s:%d\n", expr, file, line);
    wl_log(m);
    exit(3);
}

int atexit(void (*fn)(void))
{
    (void)fn;
    return 0;
}

char *getenv(const char *name)
{
    (void)name;
    return NULL;
}

/* microlib's sscanf brings its aligned allocator along, which wants these;
   it never allocates for the formats the game uses */
__attribute__((used)) char __heap_base[16];
__attribute__((used)) char __heap_limit[1];

