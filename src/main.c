/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <psp2/appmgr.h>
#include <psp2/ctrl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/vshbridge.h>
#include <taihen.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"
#include "mount_api.h"
#include "screen.h"
#include "sha256.h"
#include "storage.h"
#include "catalog.h"

#define ROOT TROPHY_OUTPUT_ROOT
#define DATA "ur0:user/00/trophy/data"
#define CONF "ur0:user/00/trophy/conf"
#define MAX_FILE (1024 * 1024)
#define ERR_IO (-0x7201)
#define ERR_ID (-0x7202)
#define ERR_CHANGED (-0x7203)
#define ERR_LIMIT (-0x7204)
#define WHITE 0xffe9e4dd
#define GOLD 0xff3fc3f6
#define RED 0xff7777ff
typedef TrophyCatalogEntry Title;
typedef struct { unsigned char *bytes; size_t size; char hash[65]; } Blob;
static TrophyCatalog catalog;
#define titles catalog.entries
#define TITLE_COUNT ((int)catalog.count)
static int selected, scan_ok, modules_ready, ready_count;
static SceUID kernel_module = -1, user_module = -1;
static int mounted;
static TrophyMountRequest active_mount;
static char status[128] = "Ready. Export does not alter installed trophy data.";
static char last_batch[128];
static int output_error, log_error;
static const char *output_failed_path;

static int log_write(void *context, const void *data, size_t size) {
    return sceIoWrite(*(SceUID *)context, data, size);
}

static void log_line(const char *format, ...) {
    char line[512];
    va_list args; va_start(args, format);
    int n = vsnprintf(line, sizeof(line), format, args); va_end(args);
    if (n < 0) return;
    size_t size = strlen(line); line[size++] = '\n';
    SceUID fd = sceIoOpen(ROOT "/diagnostic.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, TROPHY_FILE_MODE);
    if (fd < 0) { log_error = fd; return; }
    int r = trophy_write_all(log_write, &fd, line, size);
    if (r >= 0) r = sceIoSyncByFd(fd, 0);
    int closed = sceIoClose(fd);
    if (r >= 0 && closed < 0) r = closed;
    log_error = r < 0 ? r : 0;
}

static int storage_stat(void *context, const char *path) {
    SceIoStat st; memset(&st, 0, sizeof(st));
    int r = sceIoGetstat(path, &st);
    return r < 0 ? r : (SCE_S_ISDIR(st.st_mode) ? 1 : 0);
}
static int storage_mkdir(void *context, const char *path, unsigned mode) {
    return sceIoMkdir(path, mode);
}
static const TrophyStorageOps storage_ops = {storage_stat, storage_mkdir};
static int prepare_output(void) {
    output_error = trophy_prepare_output(&storage_ops, NULL, &output_failed_path);
    if (output_error < 0) {
        snprintf(status, sizeof(status), "Output init failed %08X: %s", (unsigned)output_error, output_failed_path);
        return output_error;
    }
    return 0;
}

static void blob_free(Blob *blob) { free(blob->bytes); memset(blob, 0, sizeof(*blob)); }
static int read_blob(const char *path, Blob *blob) {
    memset(blob, 0, sizeof(*blob));
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) return fd;
    SceOff length = sceIoLseek(fd, 0, SCE_SEEK_END);
    if (length <= 0 || length > MAX_FILE || sceIoLseek(fd, 0, SCE_SEEK_SET) < 0) {
        sceIoClose(fd); return ERR_LIMIT;
    }
    blob->bytes = malloc((size_t)length + 1);
    if (!blob->bytes) { sceIoClose(fd); return ERR_IO; }
    blob->size = (size_t)length;
    size_t done = 0;
    int r = 0;
    while (done < blob->size) {
        int n = sceIoRead(fd, blob->bytes + done, blob->size - done);
        if (n <= 0) { r = n < 0 ? n : ERR_IO; break; }
        done += (size_t)n;
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    }
    /* Reject a source which grew during the read as well as short reads. */
    unsigned char extra;
    if (!r && sceIoRead(fd, &extra, 1) != 0) r = ERR_CHANGED;
    int closed = sceIoClose(fd);
    if (!r && closed < 0) r = closed;
    if (r < 0) { blob_free(blob); return r; }
    blob->bytes[blob->size] = 0;
    trophy_sha256(blob->bytes, blob->size, blob->hash);
    return 0;
}

static int write_chunk(void *context, const void *data, size_t size) {
    sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    return sceIoWrite(*(SceUID *)context, data, size);
}

static int write_verified(const char *path, const void *bytes, size_t size) {
    /* Every export has a new directory; exclusive creation is still required. */
    SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, TROPHY_FILE_MODE);
    if (fd < 0) { log_line("write_open path=%s result=%08X", path, (unsigned)fd); return fd; }
    int r = trophy_write_all(write_chunk, &fd, bytes, size);
    if (!r) r = sceIoSyncByFd(fd, 0);
    int closed = sceIoClose(fd);
    if (!r && closed < 0) r = closed;
    if (r < 0) { log_line("write_sync_close path=%s result=%08X", path, (unsigned)r); return r; }
    Blob check;
    r = read_blob(path, &check);
    if (r < 0) return r;
    char hash[65]; trophy_sha256(bytes, size, hash);
    r = check.size == size && !strcmp(check.hash, hash) ? 0 : ERR_IO;
    blob_free(&check);
    return r;
}

typedef struct { SceUID fd; unsigned char bytes[4096]; size_t used; } BufferedOutput;
static int flush_buffer(BufferedOutput *out) {
    int r = trophy_write_all(write_chunk, &out->fd, out->bytes, out->used);
    if (r >= 0) out->used = 0;
    return r;
}
static int buffered_write(void *context, const void *data, size_t size) {
    BufferedOutput *out = context;
    const unsigned char *p = data;
    size_t total = size;
    while (size) {
        size_t space = sizeof(out->bytes) - out->used;
        size_t take = size < space ? size : space;
        memcpy(out->bytes + out->used, p, take); out->used += take; p += take; size -= take;
        if (out->used == sizeof(out->bytes)) { int r = flush_buffer(out); if (r < 0) return r; }
    }
    return (int)total;
}
static int write_catalog_file(const TrophyCatalog *inventory, const char *path) {
    BufferedOutput out = {0};
    out.fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, TROPHY_FILE_MODE);
    if (out.fd < 0) return out.fd;
    int r = trophy_catalog_write(inventory, buffered_write, &out);
    if (r >= 0) r = flush_buffer(&out);
    if (r >= 0) r = sceIoSyncByFd(out.fd, 0);
    int closed = sceIoClose(out.fd);
    return r < 0 ? r : (closed < 0 ? closed : 0);
}
static int save_catalog(void) {
    if (output_error < 0) return output_error;
    char path[128];
    snprintf(path, sizeof(path), ROOT "/catalog-%016llx.json", (unsigned long long)sceKernelGetSystemTimeWide());
    int r = write_catalog_file(&catalog, path);
    log_line("catalog_save result=%08X entries=%u files=%u path=%s", (unsigned)r,
             (unsigned)catalog.count, (unsigned)catalog.file_count, path);
    return r;
}
static int compare_source(const char *path, const Blob *original) {
    Blob current;
    int r = read_blob(path, &current);
    if (r < 0) return r;
    r = current.size == original->size && !strcmp(current.hash, original->hash) ? 0 : ERR_CHANGED;
    blob_free(&current);
    return r;
}

static int load_modules(void) {
    if (modules_ready) return 0;
    char title_id[16] = {0};
    int r = sceAppMgrAppParamGetString(sceKernelGetProcessId(), 12, title_id, sizeof(title_id));
    if (r < 0 || strcmp(title_id, TROPHY_TITLE_ID)) return ERR_ID;
    int search[2] = {0};
    if (_vshKernelSearchModuleByName("TrophyIOKernel", search) < 0) {
        kernel_module = taiLoadKernelModule("ux0:app/" TROPHY_TITLE_ID "/sce_module/trophy_kernel.skprx", 0, NULL);
        if (kernel_module < 0) return kernel_module;
        int started = 0;
        r = taiStartKernelModule(kernel_module, 0, NULL, 0, NULL, &started);
        if (r < 0 || started != SCE_KERNEL_START_SUCCESS) {
            int error = r < 0 ? r : ERR_IO;
            int stopped = taiStopUnloadKernelModule(kernel_module, 0, NULL, 0, NULL, NULL);
            log_line("kernel_start_cleanup result=%08X", (unsigned)stopped);
            if (stopped >= 0) kernel_module = -1;
            return error;
        }
    }
    int started = 0;
    user_module = sceKernelLoadStartModule("app0:sce_module/trophy_user.suprx", 0, NULL, 0, NULL, &started);
    if (user_module < 0 || started != SCE_KERNEL_START_SUCCESS) return user_module < 0 ? user_module : ERR_IO;
    modules_ready = 1;
    return 0;
}

static int unmount_active(void) {
    if (!mounted) return 0;
    if (!active_mount.mount_point[0]) return ERR_IO;
    int r = sceAppMgrUmount(active_mount.mount_point);
    log_line("unmount npwr=%s handle=%s result=%08X", active_mount.npwr,
             active_mount.mount_point, (unsigned)r);
    if (r >= 0) { mounted = 0; memset(&active_mount, 0, sizeof(active_mount)); }
    return r;
}

static void draw(void);
static int scan_root(TrophyCatalog *next, const char *root, unsigned source) {
    SceUID dir = sceIoDopen(root);
    if (dir < 0) {
        /* A missing root is an empty inventory; all other errors are visible. */
        if (dir == TROPHY_ERROR_NOENT) return 0;
        log_line("scan_root path=%s result=%08X", root, (unsigned)dir);
        return dir;
    }
    SceIoDirent entry; int r;
    while (1) {
        memset(&entry, 0, sizeof(entry)); r = sceIoDread(dir, &entry);
        if (r <= 0) break;
        if (!SCE_S_ISDIR(entry.d_stat.st_mode) || !strcmp(entry.d_name, ".") || !strcmp(entry.d_name, "..")) continue;
        /* Retain unexpected IDs in the catalogue instead of silently dropping them. */
        r = trophy_catalog_add(next, entry.d_name, source);
        if (r < 0) break;
    }
    int closed = sceIoDclose(dir);
    if (r >= 0 && closed < 0) r = closed;
    return r < 0 ? r : 0;
}
static int scan_file(const char *path, unsigned bit, Title *entry) {
    SceIoStat st; memset(&st, 0, sizeof(st));
    int r = sceIoGetstat(path, &st);
    if (r == TROPHY_ERROR_NOENT) return 0;
    if (r < 0) return r;
    if (!SCE_S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > MAX_FILE) return ERR_LIMIT;
    entry->data_files |= bit;
    return 0;
}
static int safe_component(const char *name) {
    return name && *name && strcmp(name, ".") && strcmp(name, "..") &&
           !strchr(name, '/') && !strchr(name, '\\') && !strchr(name, ':');
}
static Title *find_entry(TrophyCatalog *next, const char *name) {
    for (size_t i = 0; i < next->count; ++i)
        if (!strcmp(next->entries[i].directory, name)) return &next->entries[i];
    return NULL;
}
static int scan_tree(TrophyCatalog *next, const char *path, const char *owner,
                     const char *title_id, unsigned source, unsigned depth) {
    if (depth > 8) return ERR_LIMIT;
    SceUID dir = sceIoDopen(path);
    if (dir < 0) return dir;
    SceIoDirent item; int first_error = 0, r;
    while (1) {
        memset(&item, 0, sizeof(item)); r = sceIoDread(dir, &item);
        if (r <= 0) break;
        if (!strcmp(item.d_name, ".") || !strcmp(item.d_name, "..")) continue;
        char child[512];
        int n = snprintf(child, sizeof(child), "%s/%s", path, item.d_name);
        int result = 0;
        if (!safe_component(item.d_name) || n < 0 || n >= (int)sizeof(child)) result = ERR_LIMIT;
        else if (SCE_S_ISDIR(item.d_stat.st_mode)) result = scan_tree(next, child, owner, title_id, source, depth + 1);
        else if (SCE_S_ISREG(item.d_stat.st_mode)) result = trophy_catalog_add_file(next, child, owner, title_id, item.d_stat.st_size, source);
        if (result < 0 && !first_error) first_error = result;
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    }
    int closed = sceIoDclose(dir);
    if (r < 0 && !first_error) first_error = r;
    if (closed < 0 && !first_error) first_error = closed;
    return first_error;
}
static int scan_installed_root(TrophyCatalog *next, const char *root) {
    SceUID dir = sceIoDopen(root);
    if (dir == TROPHY_ERROR_NOENT || dir == (int)0x80010013u) return 0; /* absent optional device */
    if (dir < 0) return dir;
    SceIoDirent app; int r, first_error = 0;
    while (1) {
        memset(&app, 0, sizeof(app)); r = sceIoDread(dir, &app);
        if (r <= 0) break;
        if (!SCE_S_ISDIR(app.d_stat.st_mode) || !strcmp(app.d_name, ".") || !strcmp(app.d_name, "..")) continue;
        if (!safe_component(app.d_name) || strlen(app.d_name) >= 16) {
            if (!first_error) first_error = ERR_ID;
            continue;
        }
        ++next->apps_scanned;
        char path[512], title[96];
        snprintf(title, sizeof(title), "%s", app.d_name);
        snprintf(path, sizeof(path), "%s/%s/sce_sys/param.sfo", root, app.d_name);
        Blob sfo;
        if (read_blob(path, &sfo) >= 0) {
            trophy_sfo_text(sfo.bytes, sfo.size, "TITLE", title, sizeof(title)); blob_free(&sfo);
        }
        snprintf(path, sizeof(path), "%s/%s/sce_sys/trophy", root, app.d_name);
        SceUID trophy_dir = sceIoDopen(path);
        if (trophy_dir == TROPHY_ERROR_NOENT) continue; /* app has no trophy resources */
        if (trophy_dir < 0) { if (!first_error) first_error = trophy_dir; continue; }
        SceIoDirent trophy; int t;
        while (1) {
            memset(&trophy, 0, sizeof(trophy)); t = sceIoDread(trophy_dir, &trophy);
            if (t <= 0) break;
            if (!strcmp(trophy.d_name, ".") || !strcmp(trophy.d_name, "..")) continue;
            char child[512]; int n = snprintf(child, sizeof(child), "%s/%s", path, trophy.d_name);
            const char *owner = SCE_S_ISDIR(trophy.d_stat.st_mode) ? trophy.d_name : app.d_name;
            int result = safe_component(trophy.d_name) && n > 0 && n < (int)sizeof(child) ?
                         trophy_catalog_add(next, owner, TROPHY_SOURCE_PACKAGE) : ERR_LIMIT;
            if (result >= 0) {
                Title *entry = find_entry(next, owner);
                if (entry && !entry->title_id[0]) {
                    snprintf(entry->title_id, sizeof(entry->title_id), "%s", app.d_name);
                    snprintf(entry->title, sizeof(entry->title), "%s", title);
                }
                if (SCE_S_ISDIR(trophy.d_stat.st_mode)) result = scan_tree(next, child, owner, app.d_name, TROPHY_SOURCE_PACKAGE, 0);
                else if (SCE_S_ISREG(trophy.d_stat.st_mode)) result = trophy_catalog_add_file(next, child, owner, app.d_name, trophy.d_stat.st_size, TROPHY_SOURCE_PACKAGE);
            }
            if (result < 0 && !first_error) first_error = result;
        }
        int closed = sceIoDclose(trophy_dir);
        if (t < 0 && !first_error) first_error = t;
        if (closed < 0 && !first_error) first_error = closed;
        if (next->apps_scanned % 25 == 0) {
            snprintf(status, sizeof(status), "Scanning installed game trophy resources: %u apps", (unsigned)next->apps_scanned);
            draw();
        }
    }
    int closed = sceIoDclose(dir);
    if (r < 0 && !first_error) first_error = r;
    if (closed < 0 && !first_error) first_error = closed;
    return first_error;
}
static int scan_trophy_provider(TrophyCatalog *next, void *context) {
    int first_error = 0;
    const char *roots[] = {"ux0:app", "gr0:app", "ux0:patch", "ux0:rePatch"};
    for (unsigned i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
        int r = scan_installed_root(next, roots[i]);
        log_line("scan_games root=%s result=%08X", roots[i], (unsigned)r);
        if (r < 0 && !first_error) first_error = r;
    }
    int data_result = scan_root(next, DATA, TROPHY_SOURCE_DATA);
    int conf_result = scan_root(next, CONF, TROPHY_SOURCE_CONF);
    trophy_catalog_sort(next);
    for (size_t i = 0; i < next->count; ++i) {
        Title *entry = &next->entries[i];
        char path[352];
        if (entry->sources & TROPHY_SOURCE_CONF) {
            snprintf(path, sizeof(path), CONF "/%s/TROP.SFM", entry->directory);
            Blob sfm;
            int r = read_blob(path, &sfm);
            if (r < 0) entry->config_error = r;
            else {
                char config_id[32];
                entry->config_valid = trophy_xml_text((char *)sfm.bytes, "npcommid", config_id, sizeof(config_id)) >= 0 &&
                                      trophy_npwr_valid(config_id) && !strcmp(config_id, entry->directory);
                trophy_xml_text((char *)sfm.bytes, "title-name", entry->title, sizeof(entry->title));
                blob_free(&sfm);
            }
        }
        if (entry->sources & TROPHY_SOURCE_DATA) {
            snprintf(path, sizeof(path), DATA "/%s/TRPTITLE.DAT", entry->directory);
            int a = scan_file(path, TROPHY_FILE_TITLE, entry);
            snprintf(path, sizeof(path), DATA "/%s/TRPTRANS.DAT", entry->directory);
            int b = scan_file(path, TROPHY_FILE_TRANS, entry);
            entry->data_error = a < 0 ? a : b;
        }
        const char *inventory_roots[] = {DATA, CONF};
        unsigned inventory_sources[] = {TROPHY_SOURCE_DATA, TROPHY_SOURCE_CONF};
        for (unsigned j = 0; j < 2; ++j) if (entry->sources & inventory_sources[j]) {
            snprintf(path, sizeof(path), "%s/%s", inventory_roots[j], entry->directory);
            int tree_result = scan_tree(next, path, entry->directory, "", inventory_sources[j], 0);
            if (tree_result < 0 && !first_error) first_error = tree_result;
        }
        log_line("catalog dir=%s sources=%u files=%u state=%s config_error=%08X data_error=%08X",
                 entry->directory, entry->sources, entry->data_files, trophy_catalog_state(entry),
                 (unsigned)entry->config_error, (unsigned)entry->data_error);
    }
    const char *shared[] = {DATA "/sce_trop", "ur0:user/00/trophy/db"};
    for (unsigned i = 0; i < 2; ++i) {
        int r = scan_tree(next, shared[i], "shared", "", TROPHY_SOURCE_SHARED, 0);
        if (r < 0 && r != TROPHY_ERROR_NOENT && !first_error) first_error = r;
    }
    return data_result < 0 ? data_result : (conf_result < 0 ? conf_result : first_error);
}
/* Additional providers (e.g. savedata) must be explicitly registered with their
 * own discovery and export policy. This release registers only trophies. */
static const TrophyCatalogProvider trophy_provider = {"trophies", scan_trophy_provider};
static int scan_titles(void) {
    TrophyCatalog next = {0};
    int r = trophy_provider.scan(&next, NULL);
    trophy_catalog_free(&catalog); catalog = next;
    catalog.scan_error = r;
    selected = 0; ready_count = 0; scan_ok = r >= 0;
    for (int i = 0; i < TITLE_COUNT; ++i) if (trophy_catalog_exportable(&titles[i])) ++ready_count;
    log_line("scan provider=%s result=%08X entries=%d ready=%d excluded_system=%u",
             trophy_provider.name, (unsigned)r, TITLE_COUNT, ready_count, (unsigned)catalog.system_entries);
    int saved = save_catalog();
    if (saved < 0) snprintf(status, sizeof(status), "Catalogue save failed: %08X", (unsigned)saved);
    else snprintf(status, sizeof(status), "Scan %08X: %d sets, %u files; %d personal data ready.", (unsigned)r, TITLE_COUNT, (unsigned)catalog.file_count, ready_count);
    return r;
}

static void draw(void) {
    screen_clear();
    screen_text(24, 24, GOLD, "Trophy IO %s | All installed trophy resources", TROPHY_APP_VERSION);
    screen_text(24, 56, WHITE, "X: Export current   Triangle: Export all");
    screen_text(24, 80, WHITE, "Up/Down: Select  Left/Right: Page  Square: Rescan  Circle: Exit");
    screen_text(24, 112, WHITE, "%d trophy sets | %u files | %d personal data ready", TITLE_COUNT, (unsigned)catalog.file_count, ready_count);
    int first = (selected / 12) * 12;
    for (int i = first; i < TITLE_COUNT && i < first + 12; ++i)
        screen_text(24, 144 + (i - first) * 22, i == selected ? GOLD : WHITE,
                    "%c %-12.12s %-9.9s [%-11s] %.16s", i == selected ? '>' : ' ', titles[i].directory, titles[i].title_id, trophy_catalog_state(&titles[i]), titles[i].title);
    screen_text(24, 430, mounted || output_error < 0 ? RED : WHITE, "%.55s", status);
    screen_text(24, 458, WHITE, "%.55s", status + (strlen(status) > 55 ? 55 : strlen(status)));
    screen_text(24, 492, WHITE, "Output: ux0:data/trophy_io/out/");
    if (log_error < 0) screen_text(24, 516, RED, "Log unavailable: %08X", (unsigned)log_error);
    screen_present();
}

static int make_batch(char *batch, size_t size) {
    int r = prepare_output();
    if (r < 0) return r;
    /* A monotonic timer plus exclusive mkdir avoids collisions across reboots. */
    unsigned long long tick = (unsigned long long)sceKernelGetSystemTimeWide();
    for (int i = 0; i < 32; ++i) {
        snprintf(batch, size, ROOT "/out/run-%016llx-%02d", tick, i);
        r = sceIoMkdir(batch, TROPHY_DIRECTORY_MODE);
        if (r >= 0) return 0;
        SceIoStat st;
        if (sceIoGetstat(batch, &st) < 0) return r;
    }
    return ERR_IO;
}

static int export_title(const Title *title, const char *batch) {
    if (!trophy_catalog_exportable(title)) return ERR_ID;
    char game[192], data_dir[208], conf_dir[208], source[3][160], destination[224];
    Blob blobs[3] = {{0}};
    const char *names[3] = {"TRPTITLE.DAT", "TRPTRANS.DAT", "TROP.SFM"};
    snprintf(source[0], sizeof(source[0]), DATA "/%.12s/TRPTITLE.DAT", title->directory);
    snprintf(source[1], sizeof(source[1]), DATA "/%.12s/TRPTRANS.DAT", title->directory);
    snprintf(source[2], sizeof(source[2]), CONF "/%.12s/TROP.SFM", title->directory);
    int r = read_blob(source[2], &blobs[2]);
    if (r < 0) goto cleanup;
    char config_id[32];
    if (trophy_xml_text((char *)blobs[2].bytes, "npcommid", config_id, sizeof(config_id)) < 0 ||
        strcmp(config_id, title->directory)) { r = ERR_ID; goto cleanup; }
    if (mounted) { r = ERR_IO; goto cleanup; }
    memset(&active_mount, 0, sizeof(active_mount));
    memcpy(active_mount.npwr, title->directory, 13);
    r = trophyUserMount(&active_mount);
    log_line("mount npwr=%s result=%08X nid=%08X id=%X handle=%s", title->directory,
             (unsigned)r, (unsigned)active_mount.appmgr_nid, active_mount.mount_id,
             active_mount.mount_point);
    if (r < 0) goto cleanup;
    mounted = 1;
    if (!active_mount.mount_point[0]) { r = ERR_IO; goto cleanup; }
    uint32_t appmgr_nid = active_mount.appmgr_nid;
    int mount_id = active_mount.mount_id;
    for (int i = 0; i < 2; ++i) {
        r = read_blob(source[i], &blobs[i]);
        if (r < 0) goto cleanup;
        if (!trophy_data_matches_npwr(blobs[i].bytes, blobs[i].size, title->directory)) {
            r = ERR_ID; goto cleanup;
        }
    }
    snprintf(game, sizeof(game), "%s/%.12s", batch, title->directory);
    r = sceIoMkdir(game, TROPHY_DIRECTORY_MODE); if (r < 0) goto cleanup;
    snprintf(data_dir, sizeof(data_dir), "%s/data", game);
    snprintf(conf_dir, sizeof(conf_dir), "%s/conf", game);
    r = sceIoMkdir(data_dir, TROPHY_DIRECTORY_MODE); if (r < 0) goto cleanup;
    r = sceIoMkdir(conf_dir, TROPHY_DIRECTORY_MODE); if (r < 0) goto cleanup;
    for (int i = 0; i < 3; ++i) {
        snprintf(destination, sizeof(destination), "%s/%s", i == 2 ? conf_dir : data_dir, names[i]);
        r = write_verified(destination, blobs[i].bytes, blobs[i].size);
        if (r < 0) goto cleanup;
    }
    for (int i = 0; i < 3; ++i) {
        r = compare_source(source[i], &blobs[i]);
        if (r < 0) goto cleanup;
    }
    r = unmount_active();
    if (r < 0) goto cleanup;
    /* This is the completion marker. No manifest means an incomplete export. */
    char manifest[2048];
    int length = snprintf(manifest, sizeof(manifest),
        "{\n  \"schema\": 1, \"app\": \"TROPIO001\", \"version\": \"" TROPHY_APP_VERSION "\",\n"
        "  \"state\": \"export_verified\", \"npwr\": \"%s\",\n"
        "  \"mode\": \"decrypted-export\", \"mount_id\": %d, \"appmgr_nid\": \"%08X\",\n"
        "  \"files\": [\n"
        "    {\"path\": \"data/TRPTITLE.DAT\", \"bytes\": %u, \"sha256\": \"%s\"},\n"
        "    {\"path\": \"data/TRPTRANS.DAT\", \"bytes\": %u, \"sha256\": \"%s\"},\n"
        "    {\"path\": \"conf/TROP.SFM\", \"bytes\": %u, \"sha256\": \"%s\"}\n  ]\n}\n",
        title->directory, mount_id, (unsigned)appmgr_nid,
        (unsigned)blobs[0].size, blobs[0].hash, (unsigned)blobs[1].size, blobs[1].hash,
        (unsigned)blobs[2].size, blobs[2].hash);
    if (length < 0 || length >= (int)sizeof(manifest)) { r = ERR_IO; goto cleanup; }
    snprintf(destination, sizeof(destination), "%s/manifest.json.pending", game);
    r = write_verified(destination, manifest, (size_t)length);
    if (r >= 0) {
        char final_path[224];
        snprintf(final_path, sizeof(final_path), "%s/manifest.json", game);
        r = sceIoRename(destination, final_path);
    }
cleanup:
    if (mounted) {
        int cleanup_result = unmount_active();
        if (cleanup_result < 0) r = cleanup_result;
    }
    for (int i = 0; i < 3; ++i) blob_free(&blobs[i]);
    log_line("export npwr=%s result=%08X batch=%s", title->directory, (unsigned)r, batch);
    return r;
}

static int hash_resource(const char *path, uint64_t expected_size, SceUID destination, char hash[65]) {
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) return fd;
    TrophySha256 sha; trophy_sha256_init(&sha);
    unsigned char *buffer = malloc(65536);
    int r = buffer ? 0 : ERR_IO;
    uint64_t total = 0;
    while (r >= 0) {
        int n = sceIoRead(fd, buffer, 65536);
        if (n < 0) { r = n; break; }
        if (!n) break;
        total += n;
        if (total > expected_size) { r = ERR_CHANGED; break; }
        trophy_sha256_update(&sha, buffer, n);
        if (destination >= 0) r = trophy_write_all(write_chunk, &destination, buffer, n);
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    }
    free(buffer);
    int closed = sceIoClose(fd);
    if (r >= 0 && closed < 0) r = closed;
    if (r >= 0 && total != expected_size) r = ERR_CHANGED;
    if (r >= 0) trophy_sha256_finish(&sha, hash);
    return r;
}
static int copy_resource(const TrophyCatalogFile *file, const char *destination, char hash[65]) {
    SceUID fd = sceIoOpen(destination, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, TROPHY_FILE_MODE);
    if (fd < 0) return fd;
    int r = hash_resource(file->path, file->bytes, fd, hash);
    if (r >= 0) r = sceIoSyncByFd(fd, 0);
    int closed = sceIoClose(fd);
    if (r >= 0 && closed < 0) r = closed;
    char output_hash[65], source_hash[65];
    if (r >= 0) r = hash_resource(destination, file->bytes, -1, output_hash);
    if (r >= 0) r = hash_resource(file->path, file->bytes, -1, source_hash);
    if (r >= 0 && (strcmp(hash, output_hash) || strcmp(hash, source_hash))) r = ERR_CHANGED;
    return r;
}
static int has_resources(const Title *title) {
    for (size_t i = 0; i < catalog.file_count; ++i) {
        const TrophyCatalogFile *file = &catalog.files[i];
        if (!strcmp(file->owner, title->directory) &&
            (file->source == TROPHY_SOURCE_PACKAGE || (file->source == TROPHY_SOURCE_CONF && !strstr(file->path, "/sce_pfs/") && !strstr(file->path, "/sce_sys/")))) return 1;
    }
    return 0;
}
static int export_resources(const Title *title, const char *batch) {
    char game[384], path[512];
    int length = snprintf(game, sizeof(game), "%s/%s", batch, title->directory);
    if (length < 0 || length >= (int)sizeof(game)) return ERR_LIMIT;
    int r = trophy_ensure_directory(&storage_ops, NULL, game);
    if (r < 0) return r;
    snprintf(path, sizeof(path), "%s/resources", game);
    r = sceIoMkdir(path, TROPHY_DIRECTORY_MODE);
    if (r < 0) return r;
    TrophyCatalog manifest = {0};
    for (size_t i = 0; i < catalog.file_count; ++i) {
        const TrophyCatalogFile *file = &catalog.files[i];
        if (strcmp(file->owner, title->directory) ||
            !(file->source == TROPHY_SOURCE_PACKAGE || (file->source == TROPHY_SOURCE_CONF && !strstr(file->path, "/sce_pfs/") && !strstr(file->path, "/sce_sys/")))) continue;
        const char *name = strrchr(file->path, '/'); name = name ? name + 1 : file->path;
        snprintf(path, sizeof(path), "%s/resources/%06u-%.80s", game, (unsigned)i, name);
        char hash[65]; r = copy_resource(file, path, hash);
        if (r < 0) break;
        char relative[128]; snprintf(relative, sizeof(relative), "resources/%06u-%.80s", (unsigned)i, name);
        r = trophy_catalog_add_file(&manifest, relative, file->owner, file->title_id, file->bytes, file->source);
        if (r < 0) break;
        TrophyCatalogFile *receipt = &manifest.files[manifest.file_count - 1];
        snprintf(receipt->original_path, sizeof(receipt->original_path), "%s", file->path);
        memcpy(receipt->sha256, hash, sizeof(receipt->sha256));
    }
    if (r >= 0) {
        snprintf(path, sizeof(path), "%s/resources.json.pending", game);
        r = write_catalog_file(&manifest, path);
        if (r >= 0) {
            char final[512]; snprintf(final, sizeof(final), "%s/resources.json", game);
            r = sceIoRename(path, final);
        }
    }
    trophy_catalog_free(&manifest);
    log_line("resources_export id=%s result=%08X batch=%s", title->directory, (unsigned)r, batch);
    return r;
}
static void export_queue(int all) {
    if (prepare_output() < 0) return;
    if (!scan_ok || !TITLE_COUNT) { snprintf(status, sizeof(status), "No verified directory listing. Press Square to rescan."); return; }
    if (mounted) {
        int r = unmount_active();
        if (r < 0) { snprintf(status, sizeof(status), "Unmount failed: %08X. Queue stopped.", (unsigned)r); return; }
    }
    if (!all && !trophy_catalog_exportable(&titles[selected]) && !has_resources(&titles[selected])) {
        snprintf(status, sizeof(status), "%.40s: %s. No readable trophy resources.", titles[selected].directory, trophy_catalog_state(&titles[selected])); return;
    }
    int r = 0;
    if ((all && ready_count) || (!all && trophy_catalog_exportable(&titles[selected]))) {
        r = load_modules();
        if (r < 0) { log_line("module_load result=%08X", (unsigned)r); snprintf(status, sizeof(status), "Module load failed: %08X", (unsigned)r); return; }
    }
    r = make_batch(last_batch, sizeof(last_batch));
    if (r < 0) { snprintf(status, sizeof(status), "Output directory failed: %08X", (unsigned)r); return; }
    int success = 0, failed = 0, skipped = 0, last_error = 0;
    int begin = all ? 0 : selected, end = all ? TITLE_COUNT : selected + 1;
    for (int i = begin; i < end; ++i) {
        if (!trophy_catalog_exportable(&titles[i]) && !has_resources(&titles[i])) { ++skipped; continue; }
        snprintf(status, sizeof(status), "Exporting %.60s (%d/%d)...", titles[i].directory, i - begin + 1, end - begin); draw();
        r = trophy_catalog_exportable(&titles[i]) ? export_title(&titles[i], last_batch) : 0;
        if (r >= 0 && has_resources(&titles[i])) r = export_resources(&titles[i], last_batch);
        if (r < 0) { ++failed; last_error = r; } else ++success;
        if (mounted) break;
    }
    if (mounted) snprintf(status, sizeof(status), "Unmount failed %08X; stopped. See diagnostic.log.", (unsigned)r);
    else snprintf(status, sizeof(status), "Exported %d; failed %d; skipped %d. Last: %08X.", success, failed, skipped, (unsigned)last_error);
}

int main(void) {
    if (screen_init() < 0) { sceKernelExitProcess(1); return 1; }
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
    int r = prepare_output();
    if (r >= 0) log_line("start app=TROPIO001 version=%s export_only=1", TROPHY_APP_VERSION);
    int scanned = scan_titles();
    if (r >= 0 && scanned < 0) snprintf(status, sizeof(status), "Scan failed: %08X. Press Square to retry.", (unsigned)scanned);
    uint32_t previous = 0; int running = 1;
    while (running) {
        draw();
        SceCtrlData pad; memset(&pad, 0, sizeof(pad)); sceCtrlPeekBufferPositive(0, &pad, 1);
        uint32_t pressed = pad.buttons & ~previous; previous = pad.buttons;
        if ((pressed & SCE_CTRL_UP) && TITLE_COUNT) selected = (selected + TITLE_COUNT - 1) % TITLE_COUNT;
        if ((pressed & SCE_CTRL_DOWN) && TITLE_COUNT) selected = (selected + 1) % TITLE_COUNT;
        if ((pressed & SCE_CTRL_LEFT) && TITLE_COUNT) selected = selected >= 12 ? selected - 12 : 0;
        if ((pressed & SCE_CTRL_RIGHT) && TITLE_COUNT) selected = selected + 12 < TITLE_COUNT ? selected + 12 : TITLE_COUNT - 1;
        if (pressed & SCE_CTRL_SQUARE) {
            int prepared = prepare_output();
            r = scan_titles();
            if (prepared >= 0) snprintf(status, sizeof(status), "Scan result %08X; %d sets.", (unsigned)r, TITLE_COUNT);
        }
        if (pressed & SCE_CTRL_CROSS) export_queue(0);
        else if (pressed & SCE_CTRL_TRIANGLE) export_queue(1);
        if (pressed & SCE_CTRL_CIRCLE) {
            r = unmount_active();
            if (r < 0) snprintf(status, sizeof(status), "Unmount failed %08X. Circle retries cleanup.", (unsigned)r);
            else running = 0;
        }
        sceKernelDelayThread(20000);
    }
    if (user_module >= 0) {
        r = sceKernelStopUnloadModule(user_module, 0, NULL, 0, NULL, NULL);
        log_line("user_unload result=%08X", (unsigned)r);
    }
    if (kernel_module >= 0) {
        r = taiStopUnloadKernelModule(kernel_module, 0, NULL, 0, NULL, NULL);
        log_line("kernel_unload result=%08X", (unsigned)r);
    }
    trophy_catalog_free(&catalog);
    sceKernelExitProcess(0);
    return 0;
}
