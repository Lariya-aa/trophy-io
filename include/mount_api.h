/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TROPHY_IO_MOUNT_API_H
#define TROPHY_IO_MOUNT_API_H
#include <stdint.h>
#define TROPHY_TITLE_ID "TROPIO001"
typedef struct {
    char npwr[13];
    char mount_point[16];
    uint32_t appmgr_nid;
    int mount_id;
} TrophyMountRequest;
int trophyKernelMount(TrophyMountRequest *request);
int trophyUserMount(TrophyMountRequest *request);
#endif
