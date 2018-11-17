#include <assert.h>
#include <game-logic.h>
#include <image.h>
#include <nonstddef.h>
#include <platform.h>
#include <regions.h>
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

static uint8_t *region_areas;

static uint32_t *bg_image;

static int army_img_w, army_img_h;
static uint32_t *army_img[PARTY_COUNT][100];

static int region_focus_img_w, region_focus_img_h;
static uint32_t *region_focus_img;
static RegionID focused_region;


void init_game(void)
{
    init_region_list();

    fb = platform_funcs.framebuffer();
    fbw = platform_funcs.fb_width();
    fbh = platform_funcs.fb_height();
    fb_stride = platform_funcs.fb_stride();

    need_cursor_updates = platform_funcs.need_cursor_updates &&
        platform_funcs.need_cursor_updates();

    mouse_x = fbw / 2;
    mouse_y = fbh / 2;

    platform_funcs.limit_pointing_device(fbw, fbh);

    if (!load_image("/bg.png", &bg_image, &fbw, &fbh, fb_stride)) {
        printf("Failed to load background image\n");
        abort();
    }

    uint32_t *region_areas_img = NULL;
    if (!load_image("/region-areas.png", &region_areas_img, &fbw, &fbh, 0)) {
        printf("Failed to load region area image\n");
        abort();
    }

    region_areas = malloc(fbw * fbh);
    uint32_t *region_areas_img_ptr = region_areas_img;
    uint8_t *region_areas_ptr = region_areas;
    for (int x = 0; x < fbw; x++) {
        for (int y = 0; y < fbh; y++) {
            uint32_t full_val = *(region_areas_img_ptr++);
            int r = (full_val >> 16) & 0xff;
            int g = (full_val >> 8) & 0xff;
            int b = full_val & 0xff;

            assert((!r || r == 0x80 || r == 0xff) &&
                   (!g || g == 0x80 || g == 0xff) &&
                   (!b || b == 0x80 || b == 0xff));

            int region =
                (!r ? 0 : r == 0x80 ? 1 : 2) +
                (!g ? 0 : g == 0x80 ? 1 : 2) * 3 +
                (!b ? 0 : b == 0x80 ? 1 : 2) * 9;

            assert(region < (int)REGION_COUNT);

            *(region_areas_ptr++) = region;
        }
    }

    if (!load_image("/army-none.png", &army_img[RED][0],
                    &army_img_w, &army_img_h, 0))
    {
        printf("Failed to load army-none.png\n");
        abort();
    }
    army_img[BLUE][0] = army_img[RED][0];

    for (int i = 1; i < 100; i++) {
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

    if (!load_image("/focus-region.png", &region_focus_img,
                    &region_focus_img_w, &region_focus_img_h, 0))
    {
        printf("Failed to load focus-region.png\n");
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

    for (RegionID i = 1; i < REGION_COUNT; i++) {
        ablitlmt(fb, army_img[regions[i].taken_by][regions[i].troops],
                 regions[i].troops_pos.x - army_img_w / 2,
                 regions[i].troops_pos.y - army_img_h / 2,
                 army_img_w, army_img_h, fb_stride,
                 army_img_w * sizeof(uint32_t),
                 xmin, ymin, xmax, ymax);

        if (i == focused_region) {
            ablitlmt(fb, region_focus_img,
                     regions[i].troops_pos.x - region_focus_img_w / 2,
                     regions[i].troops_pos.y - region_focus_img_h / 2,
                     region_focus_img_w, region_focus_img_h,
                     fb_stride, region_focus_img_w * sizeof(uint32_t),
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

        // .get_pointing_event() returns coordinates limited by our
        // previous call to .limit_pointing_device()
        RegionID new_focused_region = region_areas[mouse_y * fbw + mouse_x];

        if (new_focused_region != focused_region) {
            int hcfw = region_focus_img_w / 2, hcfh = region_focus_img_h / 2;
            if (focused_region) {
                int tpx = regions[focused_region].troops_pos.x;
                int tpy = regions[focused_region].troops_pos.y;
                REFRESH_INCLUDE(tpx - hcfw, tpy - hcfh, tpx + hcfw, tpy + hcfh);
            }
            if (new_focused_region) {
                int tpx = regions[new_focused_region].troops_pos.x;
                int tpy = regions[new_focused_region].troops_pos.y;
                REFRESH_INCLUDE(tpx - hcfw, tpy - hcfh, tpx + hcfw, tpy + hcfh);
            }
            focused_region = new_focused_region;
        }

        if (has_button && button == 272 && !button_up) {
            if (focused_region) {
                regions[focused_region].taken_by = RED;
                regions[focused_region].troops++;

                int hcfw = region_focus_img_w / 2;
                int hcfh = region_focus_img_h / 2;
                int tpx = regions[focused_region].troops_pos.x;
                int tpy = regions[focused_region].troops_pos.y;
                REFRESH_INCLUDE(tpx - hcfw, tpy - hcfh, tpx + hcfw, tpy + hcfh);
            }
        }
    }

    if (refresh_xmax > refresh_xmin) {
        refresh(refresh_xmin, refresh_ymin, refresh_xmax, refresh_ymax);
    }
}
