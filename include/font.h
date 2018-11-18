#ifndef _FONT_H
#define _FONT_H

#include <stddef.h>
#include <stdint.h>


void init_font(void);

void font_draw_text(uint32_t *output, int owidth, int oheight, size_t ostride,
                    int x, int y, const char *text, uint32_t text_color);
void font_putchar(uint32_t *output, int owidth, int oheight, size_t ostride,
                  int x, int y, uint32_t chr, uint32_t text_color);

#endif
