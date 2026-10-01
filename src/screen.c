/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>
#include "screen.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../third_party/debugScreenFont.c"
static uint32_t *frame;
static SceUID block=-1;
int screen_init(void) {
    block=sceKernelAllocMemBlock("TrophyIOScreen",SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,2*1024*1024,NULL);
    if(block<0) return block;
    int result=sceKernelGetMemBlockBase(block,(void **)&frame);
    if(result<0) { sceKernelFreeMemBlock(block); block=-1; return result; }
    screen_clear();
    SceDisplayFrameBuf buffer={0};
    buffer.size=sizeof(buffer); buffer.base=frame; buffer.pitch=960;
    buffer.pixelformat=SCE_DISPLAY_PIXELFORMAT_A8B8G8R8; buffer.width=960; buffer.height=544;
    return sceDisplaySetFrameBuf(&buffer,SCE_DISPLAY_SETBUF_NEXTFRAME);
}
void screen_clear(void) {
    if(frame) for(int i=0;i<960*544;++i) frame[i]=0xff211a14;
}
void screen_text(int x,int y,uint32_t color,const char *format,...) {
    char buffer[512]; va_list args; va_start(args,format);
    vsnprintf(buffer,sizeof(buffer),format,args); va_end(args);
    int start=x;
    for(const unsigned char *p=(const unsigned char *)buffer;*p;++p) {
        if(*p=='\n') { y+=20; x=start; continue; }
        unsigned char character=*p<128?*p:'?';
        const unsigned char *glyph=psvDebugScreenFont.glyphs+character*8;
        for(int gy=0;gy<8;++gy) for(int gx=0;gx<8;++gx) {
            if(!(glyph[gy]&(0x80>>gx))) continue;
            for(int sy=0;sy<2;++sy) for(int sx=0;sx<2;++sx) {
                int px=x+gx*2+sx,py=y+gy*2+sy;
                if(px>=0&&px<960&&py>=0&&py<544) frame[py*960+px]=color;
            }
        }
        x+=16;
    }
}
void screen_present(void) { sceDisplayWaitVblankStart(); }
void screen_finish(void) {
    if(block>=0) { sceKernelFreeMemBlock(block); block=-1; frame=NULL; }
}
