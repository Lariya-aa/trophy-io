/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TROPHY_IO_STORAGE_H
#define TROPHY_IO_STORAGE_H
/* VitaSDK SceIoAccessMode: unsafe homebrew must explicitly grant system RW.
 * Preserve user-only access while supplying SCE_S_IRSYS | SCE_S_IWSYS. */
#define TROPHY_DIRECTORY_MODE 0706u
#define TROPHY_FILE_MODE 0606u
#define TROPHY_OUTPUT_ROOT "ux0:data/trophy_io"
#define TROPHY_ERROR_NOENT ((int)0x80010002u)
#define TROPHY_ERROR_EXIST ((int)0x80010011u)
#define TROPHY_ERROR_NOTDIR ((int)0x80010014u)
typedef int (*TrophyDirectoryStat)(void *context, const char *path);
typedef int (*TrophyDirectoryCreate)(void *context, const char *path, unsigned mode);
typedef struct {
    TrophyDirectoryStat stat; /* 1: directory, 0: other file, <0: error */
    TrophyDirectoryCreate mkdir;
} TrophyStorageOps;
int trophy_ensure_directory(const TrophyStorageOps *ops, void *context, const char *path);
int trophy_prepare_output(const TrophyStorageOps *ops, void *context, const char **failed_path);
#endif
