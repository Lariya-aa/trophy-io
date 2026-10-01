/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TROPHY_IO_SCREEN_H
#define TROPHY_IO_SCREEN_H
#include <stdint.h>
typedef struct PsvDebugScreenFont {
    unsigned char *glyphs, width, height, first, last, size_w, size_h;
} PsvDebugScreenFont;
extern PsvDebugScreenFont psvDebugScreenFont;
int screen_init(void);
void screen_clear(void);
void screen_text(int x, int y, uint32_t color, const char *format, ...);
void screen_present(void);
void screen_finish(void);
#endif
