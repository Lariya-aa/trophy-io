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

#define ROOT "ux0:data/trophy_io"
#define DATA "ur0:user/00/trophy/data"
#define CONF "ur0:user/00/trophy/conf"
#define MAX_TITLES 128
#define MAX_FILE (1024 * 1024)
#define ERR_IO (-0x7201)
#define ERR_ID (-0x7202)
#define ERR_CHANGED (-0x7203)
#define ERR_LIMIT (-0x7204)
#define WHITE 0xffe9e4dd
#define GOLD 0xff3fc3f6
#define RED 0xff7777ff
typedef struct { char npwr[13]; char title[96]; } Title;
typedef struct { unsigned char *bytes; size_t size; char hash[65]; } Blob;
static Title titles[MAX_TITLES];
static int count, selected, scan_ok, modules_ready;
static SceUID kernel_module = -1, user_module = -1;
static int mounted;
static TrophyMountRequest active_mount;
static char status[128] = "Ready. Export does not alter installed trophy data.";
static char last_batch[128];

static void log_line(const char *format, ...) {
    char line[512];
    va_list args; va_start(args, format);
    int n = vsnprintf(line, sizeof(line), format, args); va_end(args);
    if (n < 0) return;
    size_t size = strlen(line); line[size++] = '\n';
    SceUID fd = sceIoOpen(ROOT "/diagnostic.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0600);
    if (fd >= 0) { sceIoWrite(fd, line, size); sceIoClose(fd); }
}

static int ensure_dir(const char *path) {
    SceIoStat st;
    int r = sceIoGetstat(path, &st);
    if (r >= 0) return SCE_S_ISDIR(st.st_mode) ? 0 : ERR_IO;
    return sceIoMkdir(path, 0700);
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
    SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, 0600);
    if (fd < 0) return fd;
    int r = trophy_write_all(write_chunk, &fd, bytes, size);
    if (!r) r = sceIoSyncByFd(fd, 0);
    int closed = sceIoClose(fd);
    if (!r && closed < 0) r = closed;
    if (r < 0) return r;
    Blob check;
    r = read_blob(path, &check);
    if (r < 0) return r;
    char hash[65]; trophy_sha256(bytes, size, hash);
    r = check.size == size && !strcmp(check.hash, hash) ? 0 : ERR_IO;
    blob_free(&check);
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

static int title_compare(const void *a, const void *b) {
    return strcmp(((const Title *)a)->npwr, ((const Title *)b)->npwr);
}
static int scan_titles(void) {
    count = 0; selected = 0; scan_ok = 0;
    SceUID dir = sceIoDopen(DATA);
    if (dir < 0) return dir;
    SceIoDirent entry; int r;
    while (1) {
        memset(&entry, 0, sizeof(entry)); r = sceIoDread(dir, &entry);
        if (r <= 0) break;
        if (!SCE_S_ISDIR(entry.d_stat.st_mode) || !trophy_npwr_valid(entry.d_name)) continue;
        if (count == MAX_TITLES) { r = ERR_LIMIT; break; }
        Title *title = &titles[count++];
        memset(title, 0, sizeof(*title)); memcpy(title->npwr, entry.d_name, 12);
        snprintf(title->title, sizeof(title->title), "Configuration unavailable");
        char path[128]; snprintf(path, sizeof(path), CONF "/%s/TROP.SFM", title->npwr);
        Blob sfm;
        if (read_blob(path, &sfm) >= 0) {
            trophy_xml_text((char *)sfm.bytes, "title-name", title->title, sizeof(title->title));
            blob_free(&sfm);
        }
    }
    int closed = sceIoDclose(dir);
    if (r >= 0 && closed < 0) r = closed;
    if (r < 0) { count = 0; return r; }
    qsort(titles, count, sizeof(titles[0]), title_compare);
    scan_ok = 1;
    log_line("scan result=0 titles=%d", count);
    return 0;
}

static void draw(void) {
    screen_clear();
    screen_text(24, 24, GOLD, "Trophy IO 0.1 | Decrypted export");
    screen_text(24, 56, WHITE, "X: Export current   Triangle: Export all");
    screen_text(24, 80, WHITE, "Up/Down: Select   Square: Rescan   Circle: Exit");
    screen_text(24, 112, WHITE, "%d installed trophy sets", count);
    int first = (selected / 12) * 12;
    for (int i = first; i < count && i < first + 12; ++i)
        screen_text(24, 144 + (i - first) * 22, i == selected ? GOLD : WHITE,
                    "%c %s  %.39s", i == selected ? '>' : ' ', titles[i].npwr, titles[i].title);
    screen_text(24, 430, mounted ? RED : WHITE, "%.55s", status);
    screen_text(24, 458, WHITE, "%.55s", status + (strlen(status) > 55 ? 55 : strlen(status)));
    screen_text(24, 492, WHITE, "Output: ux0:data/trophy_io/out/");
    screen_present();
}

static int make_batch(char *batch, size_t size) {
    int r = ensure_dir(ROOT "/out");
    if (r < 0) return r;
    /* A monotonic timer plus exclusive mkdir avoids collisions across reboots. */
    unsigned long long tick = (unsigned long long)sceKernelGetSystemTimeWide();
    for (int i = 0; i < 32; ++i) {
        snprintf(batch, size, ROOT "/out/run-%016llx-%02d", tick, i);
        r = sceIoMkdir(batch, 0700);
        if (r >= 0) return 0;
        SceIoStat st;
        if (sceIoGetstat(batch, &st) < 0) return r;
    }
    return ERR_IO;
}

static int export_title(const Title *title, const char *batch) {
    char game[192], data_dir[208], conf_dir[208], source[3][160], destination[224];
    Blob blobs[3] = {{0}};
    const char *names[3] = {"TRPTITLE.DAT", "TRPTRANS.DAT", "TROP.SFM"};
    snprintf(source[0], sizeof(source[0]), DATA "/%s/TRPTITLE.DAT", title->npwr);
    snprintf(source[1], sizeof(source[1]), DATA "/%s/TRPTRANS.DAT", title->npwr);
    snprintf(source[2], sizeof(source[2]), CONF "/%s/TROP.SFM", title->npwr);
    int r = read_blob(source[2], &blobs[2]);
    if (r < 0) goto cleanup;
    char config_id[32];
    if (trophy_xml_text((char *)blobs[2].bytes, "npcommid", config_id, sizeof(config_id)) < 0 ||
        strcmp(config_id, title->npwr)) { r = ERR_ID; goto cleanup; }
    if (mounted) { r = ERR_IO; goto cleanup; }
    memset(&active_mount, 0, sizeof(active_mount));
    memcpy(active_mount.npwr, title->npwr, 13);
    r = trophyUserMount(&active_mount);
    log_line("mount npwr=%s result=%08X nid=%08X id=%X handle=%s", title->npwr,
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
        if (!trophy_data_matches_npwr(blobs[i].bytes, blobs[i].size, title->npwr)) {
            r = ERR_ID; goto cleanup;
        }
    }
    snprintf(game, sizeof(game), "%s/%s", batch, title->npwr);
    r = sceIoMkdir(game, 0700); if (r < 0) goto cleanup;
    snprintf(data_dir, sizeof(data_dir), "%s/data", game);
    snprintf(conf_dir, sizeof(conf_dir), "%s/conf", game);
    r = sceIoMkdir(data_dir, 0700); if (r < 0) goto cleanup;
    r = sceIoMkdir(conf_dir, 0700); if (r < 0) goto cleanup;
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
        "{\n  \"schema\": 1, \"app\": \"TROPIO001\", \"version\": \"0.1.0\",\n"
        "  \"state\": \"export_verified\", \"npwr\": \"%s\",\n"
        "  \"mode\": \"decrypted-export\", \"mount_id\": %d, \"appmgr_nid\": \"%08X\",\n"
        "  \"files\": [\n"
        "    {\"path\": \"data/TRPTITLE.DAT\", \"bytes\": %u, \"sha256\": \"%s\"},\n"
        "    {\"path\": \"data/TRPTRANS.DAT\", \"bytes\": %u, \"sha256\": \"%s\"},\n"
        "    {\"path\": \"conf/TROP.SFM\", \"bytes\": %u, \"sha256\": \"%s\"}\n  ]\n}\n",
        title->npwr, mount_id, (unsigned)appmgr_nid,
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
    log_line("export npwr=%s result=%08X batch=%s", title->npwr, (unsigned)r, batch);
    return r;
}

static void export_queue(int all) {
    if (!scan_ok || !count) { snprintf(status, sizeof(status), "No verified directory listing. Press Square to rescan."); return; }
    if (mounted) {
        int r = unmount_active();
        if (r < 0) { snprintf(status, sizeof(status), "Unmount failed: %08X. Queue stopped.", (unsigned)r); return; }
    }
    int r = load_modules();
    if (r < 0) {
        log_line("module_load result=%08X", (unsigned)r);
        snprintf(status, sizeof(status), "Module load failed: %08X. See diagnostic.log.", (unsigned)r); return;
    }
    r = make_batch(last_batch, sizeof(last_batch));
    if (r < 0) { snprintf(status, sizeof(status), "Output directory failed: %08X", (unsigned)r); return; }
    int success = 0, failed = 0, last_error = 0;
    int begin = all ? 0 : selected, end = all ? count : selected + 1;
    for (int i = begin; i < end; ++i) {
        snprintf(status, sizeof(status), "Exporting %s (%d/%d)...", titles[i].npwr, i - begin + 1, end - begin); draw();
        r = export_title(&titles[i], last_batch);
        if (r < 0) { ++failed; last_error = r; } else ++success;
        if (mounted) break;
    }
    if (mounted) snprintf(status, sizeof(status), "Unmount failed %08X; stopped. See diagnostic.log.", (unsigned)r);
    else snprintf(status, sizeof(status), "Exported %d; failed %d. Last error: %08X. See log.", success, failed, (unsigned)last_error);
}

int main(void) {
    if (screen_init() < 0) { sceKernelExitProcess(1); return 1; }
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
    int r = ensure_dir("ux0:data");
    if (r >= 0) r = ensure_dir(ROOT);
    if (r < 0) snprintf(status, sizeof(status), "Cannot create output root: %08X", (unsigned)r);
    else {
        log_line("start app=TROPIO001 version=0.1.0 export_only=1");
        r = scan_titles();
        if (r < 0) snprintf(status, sizeof(status), "Scan failed: %08X. Press Square to retry.", (unsigned)r);
    }
    uint32_t previous = 0; int running = 1;
    while (running) {
        draw();
        SceCtrlData pad; memset(&pad, 0, sizeof(pad)); sceCtrlPeekBufferPositive(0, &pad, 1);
        uint32_t pressed = pad.buttons & ~previous; previous = pad.buttons;
        if ((pressed & SCE_CTRL_UP) && count) selected = (selected + count - 1) % count;
        if ((pressed & SCE_CTRL_DOWN) && count) selected = (selected + 1) % count;
        if (pressed & SCE_CTRL_SQUARE) {
            r = scan_titles();
            snprintf(status, sizeof(status), "Scan result %08X; %d sets.", (unsigned)r, count);
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
    sceKernelExitProcess(0);
    return 0;
}
