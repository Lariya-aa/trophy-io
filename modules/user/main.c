/*
 * User-module wrapper adapted from VitaShell.
 * Copyright (C) 2015-2018, TheFloW
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Source: 81af70971ba18b8ce86215b04180f1e3d21cdfc9/modules/user/main.c
 * Modified for Trophy IO on 2026-10-01: renamed modules and forwarding
 * entry point; retained only the trophy mount request.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include <psp2/kernel/modulemgr.h>
#include "mount_api.h"
int trophyUserMount(TrophyMountRequest *request) {
    return trophyKernelMount(request);
}
int _start(SceSize args, void *argp) __attribute__((weak, alias("module_start")));
int module_start(SceSize args, void *argp) { return SCE_KERNEL_START_SUCCESS; }
int module_stop(SceSize args, void *argp) { return SCE_KERNEL_STOP_SUCCESS; }
