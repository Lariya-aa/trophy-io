/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <psp2/kernel/modulemgr.h>
#include "mount_api.h"
int trophyUserMount(TrophyMountRequest *request) {
    return trophyKernelMount(request);
}
int _start(SceSize args, void *argp) __attribute__((weak, alias("module_start")));
int module_start(SceSize args, void *argp) { return SCE_KERNEL_START_SUCCESS; }
int module_stop(SceSize args, void *argp) { return SCE_KERNEL_STOP_SUCCESS; }
