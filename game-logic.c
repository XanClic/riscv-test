#include <countries.h>
#include <game-logic.h>
#include <image.h>
#include <nonstddef.h>
#include <platform.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


const char *const party_name[PARTY_COUNT] = {
    [RED] = "red",
    [BLUE] = "blue",
};


static int mouse_x, mouse_y;
static bool need_cursor_updates;

static uint32_t *fb;
static int fbw, fbh;
size_t fb_stride;

static uint32_t *bg_image;

static int army_img_w, army_img_h;
static uint32_t *army_img[PARTY_COUNT][32];

static int country_focus_img_w, country_focus_img_h;
static uint32_t *country_focus_img;
static CountryID focused_country;


void init_game(void)
{
    init_country_list();

    fb = platform_funcs.framebuffer();
    fbw = platform_funcs.fb_width();
    fbh = platform_funcs.fb_height();
    fb_stride = platform_funcs.fb_stride();

    need_cursor_updates = platform_funcs.need_cursor_updates &&
        platform_funcs.need_cursor_updates();

    mouse_x = fbw / 2;
    mouse_y = fbh / 2;

    if (!load_image("/bg.png", &bg_image, &fbw, &fbh, fb_stride)) {
        printf("Failed to load background image\n");
        abort();
    }

    if (!load_image("/army-none.png", &army_img[RED][0],
                    &army_img_w, &army_img_h, 0))
    {
        printf("Failed to load army-none.png\n");
        abort();
    }
    army_img[BLUE][0] = army_img[RED][0];

    for (int i = 1; i < 5; i++) {
        for (Party p = 0; p < PARTY_COUNT; p++) {
            char fname[32];
            snprintf(fname, sizeof(fname), "/army-%s-%i.png", party_name[p], i);
            if (!load_image(fname, &army_img[p][i],
                            &army_img_w, &army_img_h, 0))
            {
                printf("Failed to load %s\n", fname);
                abort();
            }
        }
    }

    if (!load_image("/focus-country.png", &country_focus_img,
                    &country_focus_img_w, &country_focus_img_h, 0))
    {
        printf("Failed to load focus-country.png\n");
        abort();
    }
}


static void ablitlmt(uint32_t *dst, uint32_t *src, int dx, int dy,
                     int sw, int sh, size_t dstride, size_t sstride,
                     int xmin, int ymin, int xmax, int ymax)
{
    int sx_start = MAX(xmin - dx, 0);
    int sx_end = MIN(xmax - dx, sw);

    int sy_start = MAX(ymin - dy, 0);
    int sy_end = MIN(ymax - dy, sh);

    for (int sy = sy_start; sy < sy_end; sy++) {
        for (int sx = sx_start; sx < sx_end; sx++) {
            uint32_t s, *d, a, rb, g;

            s = ((uint32_t *)((char *)src + sy * sstride))[sx];
            d = &((uint32_t *)((char *)dst + (dy + sy) * dstride))[dx + sx];

            // Spread a to 0..256 so we can lsr 8 instead of div 0xff
            a = (s >> 24) + (s >> 31);
            rb = *d & 0xff00ff;
            g = *d & 0x00ff00;
            rb = ((((s & 0xff00ff) - rb) * a) + (rb << 8)) >> 8;
            g = ((((s & 0x00ff00) - g) * a) + (g << 8)) >> 8;
            *d = (rb & 0xff00ff) | (g & 0x00ff00) | 0xff000000;
        }
    }
}


static void refresh(int xmin, int ymin, int xmax, int ymax)
{
    for (int y = ymin; y < ymax; y++) {
        memcpy((uint32_t *)((char *)fb + y * fb_stride) + xmin,
               (uint32_t *)((char *)bg_image + y * fb_stride) + xmin,
               (xmax - xmin) * sizeof(uint32_t));
    }

    for (CountryID i = 1; i < COUNTRY_COUNT; i++) {
        ablitlmt(fb, army_img[countries[i].taken_by][countries[i].troops],
                 countries[i].troops_pos.x - army_img_w / 2,
                 countries[i].troops_pos.y - army_img_h / 2,
                 army_img_w, army_img_h, fb_stride,
                 army_img_w * sizeof(uint32_t),
                 xmin, ymin, xmax, ymax);

        if (i == focused_country) {
            ablitlmt(fb, country_focus_img,
                     countries[i].troops_pos.x - country_focus_img_w / 2,
                     countries[i].troops_pos.y - country_focus_img_h / 2,
                     country_focus_img_w, country_focus_img_h,
                     fb_stride, country_focus_img_w * sizeof(uint32_t),
                     xmin, ymin, xmax, ymax);
        }
    }

    platform_funcs.fb_flush(xmin, ymin, xmax - xmin, ymax - ymin);
}


#define REFRESH_INCLUDE(xmin, ymin, xmax, ymax) \
    do { \
        refresh_xmin = MIN(refresh_xmin, xmin); \
        refresh_ymin = MIN(refresh_ymin, ymin); \
        refresh_xmax = MAX(refresh_xmax, xmax); \
        refresh_ymax = MAX(refresh_ymax, ymax); \
    } while (0)


void handle_game(void)
{
    static bool not_first_call;
    int refresh_xmin = fbw, refresh_xmax = 0;
    int refresh_ymin = fbh, refresh_ymax = 0;

    if (!not_first_call) {
        not_first_call = true;

        REFRESH_INCLUDE(0, 0, fbw, fbh);
    }

    int key, button;
    bool has_button, up, button_up;

    if (platform_funcs.get_keyboard_event(&key, &up)) {
    }

    if (platform_funcs.get_pointing_event(&mouse_x, &mouse_y, &has_button,
                                          &button, &button_up))
    {
        if (need_cursor_updates) {
            platform_funcs.move_cursor(mouse_x, mouse_y);
        }

        CountryID new_focused_country = NULL_COUNTRY;

        if (mouse_x < 1300) {
            int min_dist = 0x7fffffff;
            for (int i = 1; i < (int)COUNTRY_COUNT; i++) {
                int dx = mouse_x - countries[i].troops_pos.x;
                int dy = mouse_y - countries[i].troops_pos.y;

                int dist = dx * dx + dy * dy;
                if (dist < min_dist) {
                    new_focused_country = i;
                    min_dist = dist;
                }
            }
        }

        if (new_focused_country != focused_country) {
            int hcfw = country_focus_img_w / 2, hcfh = country_focus_img_h / 2;
            if (focused_country) {
                int tpx = countries[focused_country].troops_pos.x;
                int tpy = countries[focused_country].troops_pos.y;
                REFRESH_INCLUDE(tpx - hcfw, tpy - hcfh, tpx + hcfw, tpy + hcfh);
            }
            if (new_focused_country) {
                int tpx = countries[new_focused_country].troops_pos.x;
                int tpy = countries[new_focused_country].troops_pos.y;
                REFRESH_INCLUDE(tpx - hcfw, tpy - hcfh, tpx + hcfw, tpy + hcfh);
            }
            focused_country = new_focused_country;
        }
    }

    if (refresh_xmax > refresh_xmin) {
        refresh(refresh_xmin, refresh_ymin, refresh_xmax, refresh_ymax);
    }
}
