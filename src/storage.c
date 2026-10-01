/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "storage.h"
int trophy_ensure_directory(const TrophyStorageOps *ops, void *context, const char *path) {
    int r = ops->stat(context, path);
    if (r == 1) return 0;
    if (r == 0) return TROPHY_ERROR_NOTDIR;
    /* Preserve permission/argument failures instead of treating them as absence. */
    if (r != TROPHY_ERROR_NOENT) return r;
    r = ops->mkdir(context, path, TROPHY_DIRECTORY_MODE);
    if (r == TROPHY_ERROR_EXIST) {
        int current = ops->stat(context, path);
        return current == 1 ? 0 : (current < 0 ? current : TROPHY_ERROR_NOTDIR);
    }
    return r < 0 ? r : 0;
}
int trophy_prepare_output(const TrophyStorageOps *ops, void *context, const char **failed_path) {
    const char *paths[] = {"ux0:data/", TROPHY_OUTPUT_ROOT, TROPHY_OUTPUT_ROOT "/out"};
    if (failed_path) *failed_path = 0;
    for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        int r = trophy_ensure_directory(ops, context, paths[i]);
        if (r < 0) {
            if (failed_path) *failed_path = paths[i];
            return r;
        }
    }
    return 0;
}
