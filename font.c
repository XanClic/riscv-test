#include <assert.h>
#include <ctype.h>
#include <font.h>
#include <nonstddef.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define REPLACEMENT_CHARACTER 0xfffd


struct NPF {
    char sig[3], version;
    uint16_t height, width;
    char name[24];
} __attribute__((packed));

struct NPFChar {
    uint32_t num;
    uint8_t rows[];
} __attribute__((packed));


static const struct NPF *font;
static int char_sz, chars;
static uint16_t char_indices[256];


void init_font(void)
{
    size_t size;

    font = stdio_get_pointer("/font.npf", &size);
    assert(font);
    assert(!strncmp(font->sig, "NPF", 3) && font->version == '2');
    assert(size >= sizeof(*font));

    char_sz = sizeof(struct NPFChar) +
        DIV_ROUND_UP(font->width, 8) * font->height;

    chars = (size - sizeof(*font)) / char_sz;

    assert(chars <= 65536);

    const struct NPFChar *c = (const struct NPFChar *)(font + 1);
    for (int i = 0; i < chars; i++) {
        if (c->num < ARRAY_SIZE(char_indices)) {
            char_indices[c->num] = i;
        }
        c = (const struct NPFChar *)((char *)c + char_sz);
    }
}


static inline struct NPFChar *get_char(int index)
{
    return (struct NPFChar *)((char *)(font + 1) + index * char_sz);
}


void font_putchar(uint32_t *output, int owidth, int oheight, size_t ostride,
                  int x, int y, uint32_t chr, uint32_t text_color)
{
    if (chr >= ARRAY_SIZE(char_indices)) {
        return;
    }
    const struct NPFChar *c = get_char(char_indices[chr]);

    if (x + font->width > owidth || y + font->height > oheight) {
        return;
    }

    int ymult = DIV_ROUND_UP(font->width, 8);
    for (int yofs = 0; yofs < font->height; yofs++) {
        for (int xofs = 0; xofs < font->width; xofs++) {
            if (!(c->rows[yofs * ymult + xofs / 8] & (1u << (xofs % 8)))) {
                continue;
            }

            uint32_t *optr = (uint32_t *)((char *)output + (y + yofs) * ostride)
                                 + x + xofs;
            *optr = text_color;
        }
    }
}


static uint32_t utf8_codepoint(const char **s)
{
    uint8_t b = *((*s)++);
    int mblen = 0;
    uint32_t chr;

    while (b & 0x80) {
        mblen++;
        b <<= 1;
    }

    if (mblen == 0) {
        chr = b;
    } else if (mblen == 1) {
        return REPLACEMENT_CHARACTER;
    } else {
        chr = b >> mblen--;
    }

    while (mblen) {
        if ((**s & 0xc0) != 0x80) {
            return REPLACEMENT_CHARACTER;
        }
        chr = (chr << 6) | (*((*s)++) & 0x3f);
    }

    return chr;
}


static const char *next_bsp(const char *text, int width)
{
    const char *last_space = NULL;

    while (width > 0 && *text) {
        const char *pre_cp = text;
        uint32_t cp = utf8_codepoint(&text);
        if (isspace(cp)) {
            last_space = pre_cp;
        }
        if (cp >= 32) {
            width -= font->width + 1;
        }
    }

    return width < 0 ? last_space : NULL;
}


void font_draw_text(uint32_t *output, int owidth, int oheight, size_t ostride,
                    int x, int y, const char *text, uint32_t text_color)
{
    assert(x < owidth && y < oheight);

    int sx = x;
    const char *next_break = next_bsp(text, owidth - x);

    while (*text) {
        const char *pre_chr = text;
        uint32_t chr = utf8_codepoint(&text);

        if (pre_chr == next_break) {
            chr = '\n';
            next_break = next_bsp(text, owidth - sx);
        }

        if (chr >= 32) {
            font_putchar(output, owidth, oheight, ostride, x, y, chr, text_color);

            x += font->width + 1;
            if (*text != '\n' && next_break != text && x + font->width > owidth)
            {
                x = sx;
                y += font->height + 1;
            }
        } else if (chr == '\n') {
            x = sx;
            y += font->height + 1;
        }
    }
}
