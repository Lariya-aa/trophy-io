/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core.h"
#include <string.h>
int trophy_write_all(TrophyWrite write, void *context, const void *data, size_t size) {
    if (!write || (!data && size)) return -0x7201;
    size_t done = 0;
    while (done < size) {
        int n = write(context, (const unsigned char *)data + done, size - done);
        if (n <= 0) return n < 0 ? n : -0x7201;
        if ((size_t)n > size - done) return -0x7201;
        done += (size_t)n;
    }
    return 0;
}
int trophy_npwr_valid(const char *name) {
    if (!name || strlen(name) != 12 || memcmp(name, "NPWR", 4) || name[9] != '_') return 0;
    for (int i = 4; i < 12; ++i) {
        if (i != 9 && (name[i] < '0' || name[i] > '9')) return 0;
    }
    return 1;
}
int trophy_data_matches_npwr(const unsigned char *data, size_t size, const char *npwr) {
    if (!data || !trophy_npwr_valid(npwr) || size < 12) return 0;
    int found = 0;
    for (size_t i = 0; i + 12 <= size; ++i) {
        if (memcmp(data + i, "NPWR", 4)) continue;
        char candidate[13];
        memcpy(candidate, data + i, 12); candidate[12] = 0;
        if (!trophy_npwr_valid(candidate)) continue;
        if (memcmp(candidate, npwr, 12)) return 0;
        found = 1;
    }
    return found;
}
int trophy_xml_text(const char *xml, const char *tag, char *out, size_t capacity) {
    if (!xml || !tag || !out || capacity == 0) return -1;
    char opening[64], closing[64];
    size_t length = strlen(tag);
    if (length == 0 || length > 55) return -1;
    opening[0] = '<'; memcpy(opening + 1, tag, length);
    opening[length + 1] = '>'; opening[length + 2] = 0;
    closing[0] = '<'; closing[1] = '/'; memcpy(closing + 2, tag, length);
    closing[length + 2] = '>'; closing[length + 3] = 0;
    const char *begin = strstr(xml, opening);
    if (!begin) return -1;
    begin += length + 2;
    const char *end = strstr(begin, closing);
    if (!end || (size_t)(end - begin) >= capacity) return -1;
    memcpy(out, begin, (size_t)(end - begin)); out[end - begin] = 0;
    return 0;
}
