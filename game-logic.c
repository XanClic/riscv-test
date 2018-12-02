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

typedef enum MainPhase {
    MAIN_WAITING_FOR_OTHER,
    MAIN_REINFORCEMENT,
    MAIN_BATTLE,
} MainPhase;


#define STATUS_X 1320
#define STATUS_PHASE_Y   20
#define STATUS_PHASE_H   40
#define STATUS_TODO_Y    60
#define STATUS_TODO_H    40
#define STATUS_INFO_Y   100
#define STATUS_INFO_H    40
#define STATUS_PROMPT_Y 140
#define STATUS_PROMPT_H  40
#define STATUS_ERROR_Y  800
#define STATUS_ERROR_H   80


typedef struct LoadedImage {
    int w, h;
    uint32_t *d;
} LoadedImage;


static int mouse_x, mouse_y;
static bool need_cursor_updates;

static uint32_t *fb;
static int fbw, fbh;
static size_t fb_stride;

static uint8_t *region_areas;

static uint32_t *bg_image;

static int army_img_w, army_img_h;
static uint32_t *army_img[PARTY_COUNT][100];

static RegionID focused_region;
static RegionID attacking_region, attacked_region;

static LoadedImage region_focus_img, attacking_region_img, attacked_region_img;
static LoadedImage error_icon;
static int region_troops_max_w, region_troops_max_h;

static int dice_w, dice_h;
static uint32_t *dice_img[6];

static GamePhase game_phase = INITIALIZATION;
static MainPhase main_phase[PARTY_COUNT];

static int troops_to_place[PARTY_COUNT];

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

enum IntegerPromptPurpose {
    NO_PROMPT,
    ATTACK_TROOPS_COUNT,
    CLAIM_TROOPS_COUNT,
};

static enum IntegerPromptPurpose integer_prompt;
static enum IntegerPromptPurpose integer_prompt_done;
static int integer_prompt_chrs = -1;
static int integer_prompt_value;


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

    if (!load_image("/error-icon.png", &error_icon.d, &error_icon.w,
                    &error_icon.h, 0))
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

    if (!load_image("/focus-region.png", &region_focus_img.d,
                    &region_focus_img.w, &region_focus_img.h, 0))
    {
        puts("Failed to load focus-region.png");
        abort();
    }
    region_troops_max_w = MAX(region_troops_max_w, region_focus_img.w);
    region_troops_max_h = MAX(region_troops_max_h, region_focus_img.h);

    if (!load_image("/attacking-region.png", &attacking_region_img.d,
                    &attacking_region_img.w, &attacking_region_img.h, 0))
    {
        puts("Failed to load attacking-region.png");
        abort();
    }
    region_troops_max_w = MAX(region_troops_max_w, attacking_region_img.w);
    region_troops_max_h = MAX(region_troops_max_h, attacking_region_img.h);

    if (!load_image("/attacked-region.png", &attacked_region_img.d,
                    &attacked_region_img.w, &attacked_region_img.h, 0))
    {
        puts("Failed to load attacked-region.png");
        abort();
    }
    region_troops_max_w = MAX(region_troops_max_w, attacked_region_img.w);
    region_troops_max_h = MAX(region_troops_max_h, attacked_region_img.h);

    for (int i = 0; i < 6; i++) {
        char fname[16];
        snprintf(fname, sizeof(fname), "/die-%i.png", i + 1);
        if (!load_image(fname, &dice_img[i], &dice_w, &dice_h, 0)) {
            printf("Failed to load %s\n", fname);
            abort();
        }
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

        if (i == attacking_region) {
            ablitlmt(fb, attacking_region_img.d,
                     regions[i].troops_pos.x - attacking_region_img.w / 2,
                     regions[i].troops_pos.y - attacking_region_img.h / 2,
                     attacking_region_img.w, attacking_region_img.h,
                     fb_stride, attacking_region_img.w * sizeof(uint32_t),
                     xmin, ymin, xmax, ymax);
        }

        if (i == attacked_region) {
            ablitlmt(fb, attacked_region_img.d,
                     regions[i].troops_pos.x - attacked_region_img.w / 2,
                     regions[i].troops_pos.y - attacked_region_img.h / 2,
                     attacked_region_img.w, attacked_region_img.h,
                     fb_stride, attacked_region_img.w * sizeof(uint32_t),
                     xmin, ymin, xmax, ymax);
        }

        if (i == focused_region) {
            ablitlmt(fb, region_focus_img.d,
                     regions[i].troops_pos.x - region_focus_img.w / 2,
                     regions[i].troops_pos.y - region_focus_img.h / 2,
                     region_focus_img.w, region_focus_img.h,
                     fb_stride, region_focus_img.w * sizeof(uint32_t),
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

    ablitlmt(fb, error_icon.d, STATUS_X, STATUS_ERROR_Y, error_icon.w,
             error_icon.h, fb_stride, error_icon.w * sizeof(uint32_t), 0, 0,
             fbw, fbh);

    font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X + error_icon.w + 10,
                   STATUS_ERROR_Y, full_msg, 0x400000);

    platform_funcs.fb_flush(STATUS_X, STATUS_ERROR_Y,
                            fbw - STATUS_X, fbh - STATUS_ERROR_Y);

    // TODO: DOOOOT sound
}


static void clear_invalid_move(void)
{
    clear_to_bg(STATUS_X, STATUS_ERROR_Y, fbw - STATUS_X, STATUS_ERROR_H);
    platform_funcs.fb_flush(STATUS_X, STATUS_ERROR_Y, fbw - STATUS_X,
                            STATUS_ERROR_H);
}


static void switch_main_phase(Party p, MainPhase new_phase)
{
    switch (new_phase) {
        case MAIN_WAITING_FOR_OTHER:
            if (p == PLAYER) {
                clear_to_bg(STATUS_X, 0, fbw - STATUS_X, fbh);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X,
                               STATUS_PHASE_Y, "Waiting for other players...",
                               0x404040);
                platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X, fbh);
            }
            break;

        case MAIN_REINFORCEMENT: {
            int regions_taken = 0;
            uint32_t continents_taken = (1u << CONTINENT_COUNT) - 1;
            for (RegionID r = 1; r < REGION_COUNT; r++) {
                if (regions[r].taken_by == p) {
                    regions_taken++;
                } else {
                    continents_taken &= ~(1u << regions[r].continent);
                }
            }
            // 2 as minimum instead of 3 because we don't have 42 regions
            troops_to_place[p] = MAX(2, regions_taken / 3);
            for (ContinentID c = 1; c < CONTINENT_COUNT; c++) {
                if (continents_taken & (1u << c)) {
                    troops_to_place[p] += continent_bonuses[c];
                }
            }

            if (p == PLAYER) {
                char buf[64];
                snprintf(buf, sizeof(buf), "You have %i troop%s remaining.",
                         troops_to_place[p],
                         troops_to_place[p] == 1 ? "" : "s");

                clear_to_bg(STATUS_X, 0, fbw - STATUS_X, fbh);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X,
                               STATUS_PHASE_Y, "Reinforcements", 0x404040);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                               "Reinforce your regions by placing troops.", 0);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_INFO_Y,
                               buf, 0);
                platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X, fbh);
            }
            break;
        }

        case MAIN_BATTLE: {
            if (p == PLAYER) {
                clear_to_bg(STATUS_X, 0, fbw - STATUS_X, fbh);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X,
                               STATUS_PHASE_Y, "Battle", 0x404040);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                               "Choose region to attack from", 0);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_INFO_Y,
                               "Press the space bar to end the battle phase",
                               0);
                platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X, fbh);
            }
            break;
        }

        default:
            abort();
    }

    main_phase[p] = new_phase;
}


static void switch_game_phase(GamePhase new_phase)
{
    switch (new_phase) {
        case INITIALIZATION:
            abort();

        case PREPARATION: {
            for (int i = 0; i < PARTY_COUNT; i++) {
                troops_to_place[i] =
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
            switch_main_phase(0, MAIN_REINFORCEMENT);
            break;

        default:
            abort();
    }

    game_phase = new_phase;
}


static RegionID ai_place_troops(Party p)
{
    if (!troops_to_place[p]) {
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
                troops_to_place[p]--;
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
                troops_to_place[p]--;
                return r;
            }
        }
    }

    abort();
}

static int ai_choose_defense_count(RegionID attacked, RegionID attacking,
                                   int attacking_count)
{
    (void)attacking;
    (void)attacking_count;

    return MIN(regions[attacked].troops, 2);
}


static int compare_ints_rev(const void *x, const void *y)
{
    return *(const int *)y - *(const int *)x;
}

// Battles and returns true if the defending region is empty after
static bool battle(RegionID attacking, RegionID defending,
                   int attacking_count, int defending_count)
{
    assert(attacking_count >= 1 && attacking_count <= 3 &&
           defending_count >= 1 && defending_count <= 2);

    int attacking_dice[3] = { -1, -1, -1 }, defending_dice[2] = { -1, -1 };

    for (int i = 0; i < attacking_count; i++) {
        attacking_dice[i] = rand() % 6;
    }
    for (int i = 0; i < defending_count; i++) {
        defending_dice[i] = rand() % 6;
    }

    // I'm lazy
    qsort(attacking_dice, attacking_count, sizeof(attacking_dice[0]),
          compare_ints_rev);
    qsort(defending_dice, defending_count, sizeof(defending_dice[0]),
          compare_ints_rev);

    int start_y = STATUS_ERROR_Y - (dice_h + 16) * 2;
    clear_to_bg(STATUS_X, start_y, fbw - STATUS_X, fbh - start_y);

    int y = start_y;
    for (int i = 0; i < attacking_count; i++) {
        int x = STATUS_X + (dice_w + 16) * i;
        ablitlmt(fb, dice_img[attacking_dice[i]], x, y, dice_w, dice_h,
                 fb_stride, dice_w * sizeof(uint32_t), STATUS_X, 0, fbw, fbh);
    }
    y += dice_h + 16;
    for (int i = 0; i < defending_count; i++) {
        int x = STATUS_X + (dice_w + 16) * i;
        ablitlmt(fb, dice_img[defending_dice[i]], x, y, dice_w, dice_h,
                 fb_stride, dice_w * sizeof(uint32_t), STATUS_X, 0, fbw, fbh);
    }


    int attacking_losses = 0, defending_losses = 0;
    for (int i = 0; i < MIN(attacking_count, defending_count); i++) {
        if (attacking_dice[i] > defending_dice[i]) {
            defending_losses++;
        } else {
            attacking_losses++;
        }
    }

    char casualties[128];
    snprintf(casualties, sizeof(casualties),
             "Attacker lost %i unit%s, defender lost %i unit%s",
             attacking_losses, attacking_losses == 1 ? "" : "s",
             defending_losses, defending_losses == 1 ? "" : "s");
    font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_ERROR_Y,
                   casualties, 0);

    regions[attacking].troops -= attacking_losses;
    regions[defending].troops -= defending_losses;

    platform_funcs.fb_flush(STATUS_X, start_y, fbw - STATUS_X, fbh - start_y);

    if (regions[defending].troops) {
        return false;
    } else {
        int troops_moved = attacking_count - attacking_losses;

        regions[defending].taken_by = regions[attacking].taken_by;
        regions[attacking].troops -= troops_moved;
        regions[defending].troops += troops_moved;
        return true;
    }
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
        int hcfw = region_troops_max_w / 2; \
        int hcfh = region_troops_max_h / 2; \
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

    int key = -1, button = -1;
    bool has_button, key_up, button_up;
    bool lbuttondown = false;

    if (platform_funcs.get_keyboard_event(&key, &key_up) && !key_up) {
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

    if (integer_prompt) {
        bool update = false;

        if (integer_prompt_chrs < 0) {
            update = true;
            integer_prompt_chrs = 0;
            integer_prompt_value = 0;
        }

        if (key >= KEY_1 && key <= KEY_0 && !key_up &&
            integer_prompt_value < 65536)
        {
            int num = key == KEY_0 ? 0 : key - KEY_1 + 1;
            integer_prompt_value = (integer_prompt_value * 10) + num;
            integer_prompt_chrs++;
            if (!integer_prompt_value) {
                // Result of the stupid design
                integer_prompt_chrs = 1;
            }
            update = true;
        } else if (key == KEY_BACKSPACE && !key_up && integer_prompt_chrs) {
            integer_prompt_value /= 10;
            integer_prompt_chrs--;
            update = true;
        } else if (key == KEY_ENTER && !key_up) {
            integer_prompt_done = integer_prompt;
            integer_prompt = NO_PROMPT;
            integer_prompt_chrs = -1;
            update = true;
        }

        if (update) {
            char prompt[32];
            if (integer_prompt_chrs > 0) {
                snprintf(prompt, sizeof(prompt), "> %i_", integer_prompt_value);
            } else if (integer_prompt) {
                snprintf(prompt, sizeof(prompt), "> _");
            } else {
                prompt[0] = 0;
            }
            clear_to_bg(STATUS_X, STATUS_PROMPT_Y, fbw - STATUS_X, STATUS_PROMPT_H);
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_PROMPT_Y,
                           prompt, 0);
            platform_funcs.fb_flush(STATUS_X, STATUS_PROMPT_Y, fbw - STATUS_X,
                                    STATUS_PROMPT_H);
        }
    }

    if ((game_phase == PREPARATION || (game_phase == MAIN &&
        main_phase[PLAYER] == MAIN_REINFORCEMENT)) && focused_region &&
        lbuttondown)
    {
        assert(troops_to_place[PLAYER] > 0);

        int unclaimed = 0;
        for (RegionID r = 1; r < REGION_COUNT; r++) {
            if (!regions[r].troops) {
                unclaimed++;
            }
        }

        if (game_phase == MAIN) {
            assert(!unclaimed && regions[focused_region].troops);
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
                            STATUS_TODO_H);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                               "Reinforce your regions by placing additional "
                               "troops.", 0);
                platform_funcs.fb_flush(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                                        STATUS_TODO_H);
            }
        }

        REFRESH_INCLUDE_REGION_TROOPS(focused_region);

        troops_to_place[PLAYER]--;

        if (unclaimed <= PARTY_COUNT) {
            char buf[64];
            snprintf(buf, sizeof(buf), "You have %i troop%s remaining.",
                     troops_to_place[PLAYER],
                     troops_to_place[PLAYER] == 1 ? "" : "s");
            clear_to_bg(STATUS_X, STATUS_INFO_Y, fbw - STATUS_X, STATUS_INFO_H);
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_INFO_Y,
                           buf, 0);
            platform_funcs.fb_flush(STATUS_X, STATUS_INFO_Y, fbw - STATUS_X,
                                    STATUS_INFO_H);
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

        if (!troops_to_place[PLAYER]) {
            for (Party p = 0; p < PARTY_COUNT; p++) {
                assert(!troops_to_place[p]);
            }

            if (game_phase == PREPARATION) {
                switch_game_phase(MAIN);
            } else {
                switch_main_phase(PLAYER, MAIN_BATTLE);
            }
        }

        goto post_logic;
    }

    if (game_phase == MAIN && main_phase[PLAYER] == MAIN_BATTLE &&
        focused_region && lbuttondown)
    {
        integer_prompt = false;

        if (!attacking_region) {
            if (regions[focused_region].taken_by != PLAYER) {
                invalid_move("This region does not belong to you");
                goto post_logic;
            } else if (regions[focused_region].troops < 2) {
                invalid_move("To attack, you need at least two units");
                goto post_logic;
            }

            attacking_region = focused_region;
            REFRESH_INCLUDE_REGION_TROOPS(attacking_region);
        } else if (attacking_region == focused_region) {
            REFRESH_INCLUDE_REGION_TROOPS(attacking_region);
            attacking_region = NULL_REGION;
            if (attacked_region) {
                REFRESH_INCLUDE_REGION_TROOPS(attacked_region);
                attacked_region = NULL_REGION;
            }
        } else if (!attacked_region) {
            if (regions[focused_region].taken_by == PLAYER) {
                invalid_move("You can only attack enemy-controlled regions");
                goto post_logic;
            }
            // No empty regions
            assert(regions[focused_region].troops);

            bool is_neighbor = false;
            for (int i = 0; i < regions[attacking_region].neighbor_count; i++) {
                if (regions[attacking_region].neighbors[i] == focused_region) {
                    is_neighbor = true;
                    break;
                }
            }
            if (!is_neighbor) {
                invalid_move("You can only attack regions which share a border "
                             "with the attacking region");
                goto post_logic;
            }

            attacked_region = focused_region;
            REFRESH_INCLUDE_REGION_TROOPS(attacked_region);
        } else if (attacked_region == focused_region) {
            REFRESH_INCLUDE_REGION_TROOPS(attacked_region);
            attacked_region = NULL_REGION;
        } else {
            invalid_move("Battles only involve two regions");
            goto post_logic;
        }

        clear_to_bg(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X, STATUS_TODO_H);
        if (!attacking_region) {
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose region to attack from", 0);
        } else if (!attacked_region) {
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose enemy-controlled region to attack", 0);
        } else {
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose how many units you want to attack with "
                           "(1, 2, or 3)", 0);
            integer_prompt = ATTACK_TROOPS_COUNT;
        }
        platform_funcs.fb_flush(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                                STATUS_TODO_H);

        goto post_logic;
    }

    if (game_phase == MAIN && main_phase[PLAYER] == MAIN_BATTLE &&
        integer_prompt_done == ATTACK_TROOPS_COUNT)
    {
        assert(attacking_region && attacked_region);

        int attacking_count = integer_prompt_value;
        integer_prompt_done = NO_PROMPT;

        if (attacking_count < 1 || attacking_count > 3) {
            invalid_move("Invalid number of attacking units");
            goto post_logic;
        } else if (regions[attacking_region].troops < attacking_count + 1) {
            invalid_move("Cannot attack with this many units (at least one "
                         "must be left behind)");
            goto post_logic;
        }

        int defending_count = ai_choose_defense_count(attacked_region,
                                                      attacking_region,
                                                      attacking_count);

        REFRESH_INCLUDE_REGION_TROOPS(attacking_region);
        REFRESH_INCLUDE_REGION_TROOPS(attacked_region);

        if (battle(attacking_region, attacked_region,
                   attacking_count, defending_count))
        {
            clear_to_bg(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X, STATUS_TODO_H);
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose how many units you want to move to the "
                           "conquered region", 0);
            platform_funcs.fb_flush(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                                    STATUS_TODO_H);

            integer_prompt = CLAIM_TROOPS_COUNT;
        } else {
            attacking_region = attacked_region = NULL_REGION;

            // I cannot be lazy and use switch_main_phase(PLAYER, MAIN_BATTLE)
            // here because that would clear the whole side bar, but the dice
            // should stay visible
            clear_to_bg(STATUS_X, 0, fbw - STATUS_X,
                        STATUS_INFO_Y + STATUS_INFO_H);
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X,
                           STATUS_PHASE_Y, "Battle", 0x404040);
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose region to attack from", 0);
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_INFO_Y,
                           "Press the space bar to end the battle phase",
                           0);
            platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X,
                                    STATUS_INFO_Y + STATUS_INFO_H);
        }

        goto post_logic;
    }

    if (game_phase == MAIN && main_phase[PLAYER] == MAIN_BATTLE &&
        integer_prompt_done == CLAIM_TROOPS_COUNT)
    {
        assert(attacking_region && attacked_region);

        int moving_troops = integer_prompt_value;
        integer_prompt_done = NO_PROMPT;

        if (regions[attacking_region].troops < moving_troops) {
            invalid_move("Not enough units in the attacking region");
            goto post_logic;
        } else if (regions[attacking_region].troops == moving_troops) {
            invalid_move("At least one unit has to stay behind");
            goto post_logic;
        }

        regions[attacking_region].troops -= moving_troops;
        regions[attacked_region].troops += moving_troops;

        REFRESH_INCLUDE_REGION_TROOPS(attacking_region);
        REFRESH_INCLUDE_REGION_TROOPS(attacked_region);
        attacking_region = attacked_region = NULL_REGION;

        // I'm lazy
        switch_main_phase(PLAYER, MAIN_BATTLE);

        goto post_logic;
    }

post_logic:
    if (refresh_xmax > refresh_xmin) {
        refresh(refresh_xmin, refresh_ymin, refresh_xmax, refresh_ymax);
    }
}
