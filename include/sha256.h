/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TROPHY_IO_SHA256_H
#define TROPHY_IO_SHA256_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t state[8];
    uint64_t bytes;
    unsigned char block[64];
    size_t used;
} TrophySha256;
void trophy_sha256_init(TrophySha256 *ctx);
void trophy_sha256_update(TrophySha256 *ctx, const void *data, size_t size);
void trophy_sha256_finish(TrophySha256 *ctx, char hex[65]);
void trophy_sha256(const void *data, size_t size, char hex[65]);
#endif
