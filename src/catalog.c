/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "catalog.h"
#include "core.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
void trophy_catalog_free(TrophyCatalog *catalog) {
    free(catalog->entries);
    free(catalog->files);
    memset(catalog, 0, sizeof(*catalog));
}
int trophy_catalog_add(TrophyCatalog *catalog, const char *directory, unsigned source) {
    if (!directory || !*directory || strlen(directory) >= 256 ||
        strchr(directory, '/') || strchr(directory, '\\') || strchr(directory, ':') ||
        !strcmp(directory, ".") || !strcmp(directory, "..") ||
        (source != TROPHY_SOURCE_DATA && source != TROPHY_SOURCE_CONF && source != TROPHY_SOURCE_PACKAGE))
        return TROPHY_CATALOG_ERR_NAME;
    /* Shared account state is not a per-game trophy container. */
    if (!strcmp(directory, "sce_trop")) { ++catalog->system_entries; return 0; }
    for (size_t i = 0; i < catalog->count; ++i) {
        if (!strcmp(catalog->entries[i].directory, directory)) {
            catalog->entries[i].sources |= source;
            return 0;
        }
    }
    if (catalog->count == catalog->capacity) {
        size_t capacity = catalog->capacity ? catalog->capacity * 2 : 32;
        if (capacity < catalog->capacity || capacity > SIZE_MAX / sizeof(*catalog->entries))
            return TROPHY_CATALOG_ERR_MEMORY;
        void *entries = realloc(catalog->entries, capacity * sizeof(*catalog->entries));
        if (!entries) return TROPHY_CATALOG_ERR_MEMORY;
        catalog->entries = entries; catalog->capacity = capacity;
    }
    TrophyCatalogEntry *entry = &catalog->entries[catalog->count++];
    memset(entry, 0, sizeof(*entry));
    strcpy(entry->directory, directory);
    strcpy(entry->title, "Configuration unavailable");
    entry->sources = source;
    return 0;
}
static int compare_entry(const void *a, const void *b) {
    return strcmp(((const TrophyCatalogEntry *)a)->directory,
                  ((const TrophyCatalogEntry *)b)->directory);
}
void trophy_catalog_sort(TrophyCatalog *catalog) {
    if (catalog->count > 1) qsort(catalog->entries, catalog->count, sizeof(*catalog->entries), compare_entry);
}
int trophy_catalog_exportable(const TrophyCatalogEntry *entry) {
    return trophy_npwr_valid(entry->directory) &&
           (entry->sources & (TROPHY_SOURCE_DATA | TROPHY_SOURCE_CONF)) == (TROPHY_SOURCE_DATA | TROPHY_SOURCE_CONF) &&
           entry->data_files == (TROPHY_FILE_TITLE | TROPHY_FILE_TRANS) &&
           entry->config_valid && !entry->config_error && !entry->data_error;
}
const char *trophy_catalog_state(const TrophyCatalogEntry *entry) {
    if (trophy_catalog_exportable(entry)) return "Ready";
    if (entry->sources == TROPHY_SOURCE_PACKAGE) return "Game files";
    if (!trophy_npwr_valid(entry->directory)) return "Unknown ID";
    if (entry->config_error || entry->data_error) return "Read error";
    if (!(entry->sources & TROPHY_SOURCE_CONF)) return "No config";
    if (!entry->config_valid) return "Bad config";
    if (!(entry->sources & TROPHY_SOURCE_DATA)) return "Config only";
    if (entry->data_files != (TROPHY_FILE_TITLE | TROPHY_FILE_TRANS)) return "Incomplete";
    return "Ready";
}
int trophy_catalog_add_file(TrophyCatalog *catalog, const char *path, const char *owner,
                            const char *title_id, uint64_t bytes, unsigned source) {
    if (!path || !owner || !title_id || strlen(path) >= 512 || strlen(owner) >= 256 || strlen(title_id) >= 16)
        return TROPHY_CATALOG_ERR_NAME;
    if (catalog->file_count == catalog->file_capacity) {
        size_t capacity = catalog->file_capacity ? catalog->file_capacity * 2 : 64;
        if (capacity < catalog->file_capacity || capacity > SIZE_MAX / sizeof(*catalog->files))
            return TROPHY_CATALOG_ERR_MEMORY;
        void *files = realloc(catalog->files, capacity * sizeof(*catalog->files));
        if (!files) return TROPHY_CATALOG_ERR_MEMORY;
        catalog->files = files; catalog->file_capacity = capacity;
    }
    TrophyCatalogFile *file = &catalog->files[catalog->file_count++];
    memset(file, 0, sizeof(*file));
    strcpy(file->path, path); strcpy(file->owner, owner); strcpy(file->title_id, title_id);
    file->bytes = bytes; file->source = source;
    return 0;
}
static int json_text(TrophyWrite write, void *context, const char *text) {
    int r = trophy_write_all(write, context, "\"", 1);
    for (const unsigned char *p = (const unsigned char *)text; r >= 0 && *p; ++p) {
        if (*p == '"' || *p == '\\') {
            char escaped[2] = {'\\', (char)*p}; r = trophy_write_all(write, context, escaped, 2);
        } else if (*p < 32) {
            char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
            r = trophy_write_all(write, context, escaped, 6);
        } else r = trophy_write_all(write, context, p, 1);
    }
    return r < 0 ? r : trophy_write_all(write, context, "\"", 1);
}
int trophy_catalog_write(const TrophyCatalog *catalog, TrophyWrite write, void *context) {
    char buffer[256];
    int n = snprintf(buffer, sizeof(buffer), "{\n\"schema\":1,\"scope\":\"installed-game-trophy-resources\",\"complete\":%s,\"scan_error\":%d,\"apps_scanned\":%u,\"entries\":[\n",
                     catalog->scan_error < 0 ? "false" : "true", catalog->scan_error, (unsigned)catalog->apps_scanned);
    int r = trophy_write_all(write, context, buffer, n);
#define EMIT(s) do { if (r >= 0) r = trophy_write_all(write, context, (s), strlen(s)); } while (0)
#define QUOTE(s) do { if (r >= 0) r = json_text(write, context, (s)); } while (0)
    for (size_t i = 0; r >= 0 && i < catalog->count; ++i) {
        const TrophyCatalogEntry *entry = &catalog->entries[i];
        if (i) EMIT(",\n");
        EMIT("{\"id\":"); QUOTE(entry->directory); EMIT(",\"title\":"); QUOTE(entry->title);
        EMIT(",\"title_id\":"); QUOTE(entry->title_id); EMIT(",\"state\":"); QUOTE(trophy_catalog_state(entry));
        n = snprintf(buffer, sizeof(buffer), ",\"sources\":%u}", entry->sources); EMIT(buffer);
    }
    EMIT("\n],\"files\":[\n");
    for (size_t i = 0; r >= 0 && i < catalog->file_count; ++i) {
        const TrophyCatalogFile *file = &catalog->files[i];
        if (i) EMIT(",\n");
        EMIT("{\"path\":"); QUOTE(file->path); EMIT(",\"owner\":"); QUOTE(file->owner);
        EMIT(",\"title_id\":"); QUOTE(file->title_id);
        EMIT(",\"source_path\":"); QUOTE(file->original_path);
        EMIT(",\"sha256\":"); QUOTE(file->sha256);
        n = snprintf(buffer, sizeof(buffer), ",\"bytes\":%llu,\"source\":%u}", (unsigned long long)file->bytes, file->source); EMIT(buffer);
    }
    EMIT("\n]}\n");
#undef EMIT
#undef QUOTE
    return r;
}
static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint16_t le16(const unsigned char *p) { return p[0] | (uint16_t)p[1] << 8; }
int trophy_sfo_text(const unsigned char *data, size_t size, const char *key, char *out, size_t capacity) {
    if (!data || !key || !out || !capacity || size < 20 || memcmp(data, "\0PSF", 4)) return -1;
    uint32_t keys = le32(data + 8), values = le32(data + 12), count = le32(data + 16);
    if (count > (size - 20) / 16 || keys >= size || values >= size) return -1;
    for (uint32_t i = 0; i < count; ++i) {
        const unsigned char *entry = data + 20 + 16 * i;
        size_t offset = le16(entry), length = le32(entry + 4), value = le32(entry + 12);
        if (offset >= size - keys || !memchr(data + keys + offset, 0, size - keys - offset)) return -1;
        if (strcmp((const char *)data + keys + offset, key)) continue;
        if (le16(entry + 2) != 0x204 || value >= size - values || !length || length > size - values - value) return -1;
        const unsigned char *begin = data + values + value;
        const unsigned char *end = memchr(begin, 0, length);
        if (!end || (size_t)(end - begin) >= capacity) return -1;
        memcpy(out, begin, end - begin); out[end - begin] = 0; return 0;
    }
    return -1;
}
