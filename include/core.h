/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TROPHY_IO_CORE_H
#define TROPHY_IO_CORE_H
#include <stddef.h>
int trophy_npwr_valid(const char *name);
int trophy_data_matches_npwr(const unsigned char *data, size_t size, const char *npwr);
int trophy_xml_text(const char *xml, const char *tag, char *out, size_t capacity);
typedef int (*TrophyWrite)(void *context, const void *data, size_t size);
int trophy_write_all(TrophyWrite write, void *context, const void *data, size_t size);
#endif
