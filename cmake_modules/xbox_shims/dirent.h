#ifndef _XBOX_SHIM_DIRENT_H
#define _XBOX_SHIM_DIRENT_H

#define DT_REG 1
#define DT_DIR 2

struct dirent {
    char d_name[260];
    unsigned char d_type;
};

typedef struct DIR DIR;

#ifdef __cplusplus
extern "C" {
#endif

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
int scandir(const char *dirp, struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **));
int alphasort(const struct dirent **a, const struct dirent **b);

#ifdef __cplusplus
}
#endif

#endif // _XBOX_SHIM_DIRENT_H
