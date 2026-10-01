/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TROPHY_IO_CATALOG_H
#define TROPHY_IO_CATALOG_H
#include <stddef.h>
#include <stdint.h>
#include "core.h"
#define TROPHY_SOURCE_DATA 1u
#define TROPHY_SOURCE_CONF 2u
#define TROPHY_SOURCE_PACKAGE 4u
#define TROPHY_SOURCE_SHARED 8u
#define TROPHY_FILE_TITLE 1u
#define TROPHY_FILE_TRANS 2u
#define TROPHY_CATALOG_ERR_MEMORY (-0x7301)
#define TROPHY_CATALOG_ERR_NAME (-0x7302)
typedef struct {
    char directory[256];
    char title[96];
    char title_id[16];
    unsigned sources, data_files;
    int config_valid, config_error, data_error;
} TrophyCatalogEntry;
typedef struct {
    char path[512], owner[256], title_id[16], original_path[512], sha256[65];
    uint64_t bytes;
    unsigned source;
} TrophyCatalogFile;
typedef struct {
    TrophyCatalogEntry *entries;
    size_t count, capacity, system_entries;
    TrophyCatalogFile *files;
    size_t file_count, file_capacity, apps_scanned;
    int scan_error;
} TrophyCatalog;
/* Discovery is separate from UI and export policy. A future savedata provider
 * gets its own scan callback and export policy; it is not registered yet. */
typedef struct {
    const char *name;
    int (*scan)(TrophyCatalog *catalog, void *context);
} TrophyCatalogProvider;
void trophy_catalog_free(TrophyCatalog *catalog);
int trophy_catalog_add(TrophyCatalog *catalog, const char *directory, unsigned source);
void trophy_catalog_sort(TrophyCatalog *catalog);
int trophy_catalog_exportable(const TrophyCatalogEntry *entry);
const char *trophy_catalog_state(const TrophyCatalogEntry *entry);
int trophy_catalog_add_file(TrophyCatalog *catalog, const char *path, const char *owner,
                            const char *title_id, uint64_t bytes, unsigned source);
int trophy_catalog_write(const TrophyCatalog *catalog, TrophyWrite write, void *context);
int trophy_sfo_text(const unsigned char *data, size_t size, const char *key, char *out, size_t capacity);
#endif
