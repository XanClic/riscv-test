#include <assert.h>
#include <font.h>
#include <game-logic.h>
#include <image.h>
#include <keycodes.h>
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

typedef enum GamePhase {
    INITIALIZATION,
    PREPARATION,
    MAIN,
} GamePhase;


#define STATUS_X 1320
#define STATUS_PHASE_Y 20
#define STATUS_TODO_Y  60
#define STATUS_INFO_Y  100
#define STATUS_ERROR_Y 800


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

static int error_icon_w, error_icon_h;
static uint32_t *error_icon;

static GamePhase game_phase = INITIALIZATION;

// Data for the preparation phase
static struct {
    int troops_to_place[PARTY_COUNT];
} prep;

// for 42 regions (for simplicity, we just scale down, rounding up)
static const int initial_troops_reference[] = {
    0,  // 1 player is not allowed
    45, // no source for this
    35,
    30,
    25,
};

_Static_assert(PARTY_COUNT > 1 &&
               PARTY_COUNT < ARRAY_SIZE(initial_troops_reference),
               "Invalid party count");


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
        puts("Failed to load background image");
        abort();
    }

    uint32_t *region_areas_img = NULL;
    if (!load_image("/region-areas.png", &region_areas_img, &fbw, &fbh, 0)) {
        puts("Failed to load region area image");
        abort();
    }

    if (!load_image("/error-icon.png", &error_icon, &error_icon_w,
                    &error_icon_h, 0))
    {
        puts("Failed to load error icon");
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
        puts("Failed to load army-none.png");
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
        puts("Failed to load focus-region.png");
        abort();
    }
}


static void clear_to_bg(int x, int y, int w, int h)
{
    if (x >= fbw || y >= fbh) {
        return;
    }

    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }

    w = MIN(fbw - x, w);
    h = MIN(fbh - y, h);

    if (w <= 0 || h <= 0) {
        return;
    }

    for (int yofs = 0; yofs < h; yofs++) {
        memcpy((char *)(fb + x) + (y + yofs) * fb_stride,
               (char *)(bg_image + x) + (y + yofs) * fb_stride,
               w * sizeof(uint32_t));
    }

    platform_funcs.fb_flush(x, y, w, h);
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
    xmax = MIN(xmax, STATUS_X);
    if (xmin >= STATUS_X) {
        return;
    }

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


static void invalid_move(const char *message)
{
    char full_msg[256];

    snprintf(full_msg, sizeof(full_msg), "Invalid move: %s", message);
    puts(full_msg);

    ablitlmt(fb, error_icon, STATUS_X, STATUS_ERROR_Y, error_icon_w,
             error_icon_h, fb_stride, error_icon_w * sizeof(uint32_t), 0, 0,
             fbw, fbh);

    font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X + error_icon_w + 10,
                   STATUS_ERROR_Y, full_msg, 0x400000);

    platform_funcs.fb_flush(STATUS_X, STATUS_ERROR_Y,
                            fbw - STATUS_X, fbh - STATUS_ERROR_Y);

    // TODO: DOOOOT sound
}


static void clear_invalid_move(void)
{
    clear_to_bg(STATUS_X, STATUS_ERROR_Y, fbw - STATUS_X, fbh - STATUS_ERROR_Y);
}


static void switch_game_phase(GamePhase new_phase)
{
    switch (new_phase) {
        case INITIALIZATION:
            abort();

        case PREPARATION: {
            for (int i = 0; i < PARTY_COUNT; i++) {
                prep.troops_to_place[i] =
                    DIV_ROUND_UP(initial_troops_reference[PARTY_COUNT]
                                 * REGION_COUNT,
                                 42);
            }

            clear_to_bg(STATUS_X, 0, fbw - STATUS_X, fbh);

            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_PHASE_Y,
                           "Preparation phase", 0x404040);

            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Claim regions by placing troops in them.", 0);

            platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X, fbh);

            break;
        }

        case MAIN:
            clear_to_bg(STATUS_X, 0, fbw - STATUS_X, fbh);

            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_PHASE_Y,
                           "Main phase", 0x404040);

            platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X, fbh);

            break;

        default:
            abort();
    }

    game_phase = new_phase;
}


static RegionID ai_place_troops(Party p)
{
    if (!prep.troops_to_place[p]) {
        return NULL_REGION;
    }

    int empty_region_count = 0, my_region_count = 0;
    for (RegionID r = 1; r < REGION_COUNT; r++) {
        if (!regions[r].troops) {
            empty_region_count++;
        } else if (regions[r].taken_by == p) {
            my_region_count++;
        }
    }

    if (empty_region_count) {
        int empty_region_index = rand() % empty_region_count;

        for (RegionID r = 1; r < REGION_COUNT; r++) {
            if (!regions[r].troops && !empty_region_index--) {
                regions[r].taken_by = p;
                regions[r].troops = 1;
                prep.troops_to_place[p]--;
                return r;
            }
        }
    } else if (my_region_count) {
        int my_region_index = rand() % my_region_count;

        for (RegionID r = 1; r < REGION_COUNT; r++) {
            if (regions[r].troops && regions[r].taken_by == p &&
                !my_region_index--)
            {
                regions[r].troops++;
                prep.troops_to_place[p]--;
                return r;
            }
        }
    }

    abort();
}


#define REFRESH_INCLUDE(xmin, ymin, xmax, ymax) \
    do { \
        refresh_xmin = MIN(refresh_xmin, xmin); \
        refresh_ymin = MIN(refresh_ymin, ymin); \
        refresh_xmax = MAX(refresh_xmax, xmax); \
        refresh_ymax = MAX(refresh_ymax, ymax); \
    } while (0)

#define REFRESH_INCLUDE_REGION_TROOPS(r) \
    do { \
        int hcfw = region_focus_img_w / 2; \
        int hcfh = region_focus_img_h / 2; \
        int tpx = regions[(r)].troops_pos.x; \
        int tpy = regions[(r)].troops_pos.y; \
        REFRESH_INCLUDE(tpx - hcfw, tpy - hcfh, tpx + hcfw, tpy + hcfh); \
    } while (0)


void handle_game(void)
{
    int refresh_xmin = fbw, refresh_xmax = 0;
    int refresh_ymin = fbh, refresh_ymax = 0;

    if (game_phase == INITIALIZATION) {
        REFRESH_INCLUDE(0, 0, fbw, fbh);

        switch_game_phase(PREPARATION);
    }

    int key, button;
    bool has_button, up, button_up;
    bool lbuttondown = false;

    if (platform_funcs.get_keyboard_event(&key, &up)) {
        clear_invalid_move();
    }

    if (platform_funcs.get_pointing_event(&mouse_x, &mouse_y, &has_button,
                                          &button, &button_up))
    {
        if (need_cursor_updates) {
            platform_funcs.move_cursor(mouse_x, mouse_y);
        }

        if (has_button && !button_up) {
            clear_invalid_move();
        }

        // .get_pointing_event() returns coordinates limited by our
        // previous call to .limit_pointing_device()
        RegionID new_focused_region = region_areas[mouse_y * fbw + mouse_x];

        if (new_focused_region != focused_region) {
            if (focused_region) {
                REFRESH_INCLUDE_REGION_TROOPS(focused_region);
            }
            if (new_focused_region) {
                REFRESH_INCLUDE_REGION_TROOPS(new_focused_region);
            }
            focused_region = new_focused_region;
        }

        lbuttondown = has_button && button == BTN_LEFT && !button_up;
    }

    if (game_phase == PREPARATION && focused_region && lbuttondown) {
        assert(prep.troops_to_place[PLAYER] > 0);

        int unclaimed = 0;
        for (RegionID r = 1; r < REGION_COUNT; r++) {
            if (!regions[r].troops) {
                unclaimed++;
            }
        }

        if (regions[focused_region].troops) {
            if (regions[focused_region].taken_by != PLAYER) {
                invalid_move("Cannot place troops on enemy-controlled region.");
                goto post_logic;
            }

            if (unclaimed) {
                invalid_move("All regions must be taken before you can "
                             "place reinforcements.");
                goto post_logic;
            }

            regions[focused_region].troops++;
        } else {
            regions[focused_region].taken_by = PLAYER;
            regions[focused_region].troops = 1;

            if (unclaimed <= PARTY_COUNT) {
                clear_to_bg(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                            STATUS_INFO_Y - STATUS_TODO_Y);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                               "Reinforce your regions by placing additional "
                               "troops.", 0);
                platform_funcs.fb_flush(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                                        STATUS_INFO_Y - STATUS_TODO_Y);
            }
        }

        REFRESH_INCLUDE_REGION_TROOPS(focused_region);

        prep.troops_to_place[PLAYER]--;

        if (unclaimed <= PARTY_COUNT) {
            char buf[64];
            snprintf(buf, sizeof(buf), "You have %i troop%s remaining.",
                     prep.troops_to_place[PLAYER],
                     prep.troops_to_place[PLAYER] == 1 ? "" : "s");
            clear_to_bg(STATUS_X, STATUS_INFO_Y, fbw - STATUS_X,
                        STATUS_ERROR_Y - STATUS_INFO_Y);
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_INFO_Y,
                           buf, 0);
            platform_funcs.fb_flush(STATUS_X, STATUS_INFO_Y, fbw - STATUS_X,
                                    STATUS_ERROR_Y - STATUS_INFO_Y);
        }


        for (Party p = 0; p < PARTY_COUNT; p++) {
            if (p == PLAYER) {
                continue;
            }
            RegionID r = ai_place_troops(p);
            if (r) {
                REFRESH_INCLUDE_REGION_TROOPS(r);
            }
        }

        if (!prep.troops_to_place[PLAYER]) {
            for (Party p = 0; p < PARTY_COUNT; p++) {
                assert(!prep.troops_to_place[p]);
            }

            switch_game_phase(MAIN);
        }
    }

post_logic:
    if (refresh_xmax > refresh_xmin) {
        refresh(refresh_xmin, refresh_ymin, refresh_xmax, refresh_ymax);
    }
}
