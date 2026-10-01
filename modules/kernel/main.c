/*
 * Mount bridge derived from VitaShell, Copyright (C) 2015-2018 TheFloW.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Source: 81af70971ba18b8ce86215b04180f1e3d21cdfc9/modules/kernel/main.c
 * Only trophy mounting remains; no USB, storage redirection or RIF key APIs.
 */
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/processmgr.h>
#include <taihen.h>
#include <string.h>
#include <stdio.h>
#include "mount_api.h"
#include "core.h"

int module_get_export_func(SceUID pid, const char *modname, uint32_t libnid,
                          uint32_t funcnid, uintptr_t *func);
int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t *addr);

static int mount_on_stack(TrophyMountRequest *request) {
    void *(*find_info)(void *, SceUID) = NULL;
    int (*mount_by_id)(SceUID, void *, int, const char *, const char *,
                       const char *, const void *, char *) = NULL;
    int (*get_info)(SceUID, SceUID, SceKernelModuleInfo *) = NULL;
    tai_module_info_t tai_info;
    memset(&tai_info, 0, sizeof(tai_info));
    tai_info.size = sizeof(tai_info);
    int res = taiGetModuleInfoForKernel(KERNEL_PID, "SceAppMgr", &tai_info);
    if (res < 0) return res;
    request->appmgr_nid = tai_info.module_nid;
    size_t find_offset, mount_offset;
    switch (tai_info.module_nid) {
        case 0x94CEFE4B: case 0xDFBC288C:
            find_offset = 0x2DE1; mount_offset = 0x19E15; break;
        case 0xDBB29DB7:
            find_offset = 0x2DE1; mount_offset = 0x19B51; break;
        case 0x1C9879D6:
            find_offset = 0x2DE1; mount_offset = 0x19E61; break;
        case 0x54E2E984: case 0xC3C538DE:
            find_offset = 0x2DE1; mount_offset = 0x19E6D; break;
        case 0x321E4852: case 0x700DA0CD: case 0xF7846B4E:
        case 0xA8E80BA8: case 0xB299D195:
            find_offset = 0x2DE9; mount_offset = 0x19E95; break;
        default: return -0x7101;
    }
    res = module_get_offset(KERNEL_PID, tai_info.modid, 0, find_offset, (uintptr_t *)&find_info);
    if (res < 0 || !find_info) return res < 0 ? res : -0x7102;
    res = module_get_offset(KERNEL_PID, tai_info.modid, 0, mount_offset, (uintptr_t *)&mount_by_id);
    if (res < 0 || !mount_by_id) return res < 0 ? res : -0x7102;
    res = module_get_export_func(KERNEL_PID, "SceKernelModulemgr", 0xC445FA63,
                                0xD269F915, (uintptr_t *)&get_info);
    if (res < 0)
        res = module_get_export_func(KERNEL_PID, "SceKernelModulemgr", 0x92C9FFC2,
                                    0xDAA90093, (uintptr_t *)&get_info);
    if (res < 0 || !get_info) return res < 0 ? res : -0x7102;
    SceKernelModuleInfo module_info;
    memset(&module_info, 0, sizeof(module_info));
    module_info.size = sizeof(module_info);
    res = get_info(KERNEL_PID, tai_info.modid, &module_info);
    if (res < 0) return res;
    SceUID pid = ksceKernelGetProcessId();
    unsigned char *data = (unsigned char *)module_info.segments[1].vaddr;
    if (!data) return -0x7103;
    void *process_info = find_info(data + 0x500, pid);
    if (!process_info) return -0x7103;
    char path[96];
    snprintf(path, sizeof(path), "ur0:user/00/trophy/data/%s", request->npwr);
    unsigned char key[16] = {0};
    char mount_point[16] = {0};
    request->mount_id = 0x12F;
    res = mount_by_id(pid, (unsigned char *)process_info + 0x580, 0x12F,
                      TROPHY_TITLE_ID, path, NULL, key, mount_point);
    if (res >= 0) {
        memcpy(request->mount_point, mount_point, 15);
        request->mount_point[15] = 0;
    }
    return res;
}

int trophyKernelMount(TrophyMountRequest *user_request) {
    uint32_t state;
    ENTER_SYSCALL(state);
    TrophyMountRequest request;
    memset(&request, 0, sizeof(request));
    int res = ksceKernelMemcpyUserToKernel(&request, user_request, sizeof(request));
    if (res >= 0) {
        if (request.npwr[12] != 0 || !trophy_npwr_valid(request.npwr)) {
            res = -0x7104;
        } else {
            memset(request.mount_point, 0, sizeof(request.mount_point));
            request.appmgr_nid = 0;
            request.mount_id = 0;
            res = ksceKernelRunWithStack(0x2000, (void *)mount_on_stack, &request);
            int copied = ksceKernelMemcpyKernelToUser(user_request, &request, sizeof(request));
            if (copied < 0 && res >= 0) res = copied;
        }
    }
    EXIT_SYSCALL(state);
    return res;
}

int _start(SceSize args, void *argp) __attribute__((weak, alias("module_start")));
int module_start(SceSize args, void *argp) { return SCE_KERNEL_START_SUCCESS; }
int module_stop(SceSize args, void *argp) { return SCE_KERNEL_STOP_SUCCESS; }
