#ifndef _XBOX_SHIM_SYS_STAT_H
#define _XBOX_SHIM_SYS_STAT_H

#define _S_IREAD  0x0100
#define _S_IWRITE 0x0080

#define S_IFDIR 0x4000
#define S_IFREG 0x8000
#define S_ISDIR(m) (((m) & S_IFDIR) != 0)
#define S_ISREG(m) (((m) & S_IFREG) != 0)

struct stat {
    unsigned int st_mode;
    long st_size;
    long st_mtime;
};

#ifdef __cplusplus
extern "C" {
#endif

int stat(const char *path, struct stat *buf);
int mkdir(const char *path, int mode);
int rmdir(const char *path);

#ifdef __cplusplus
}
#endif

#endif // _XBOX_SHIM_SYS_STAT_H
