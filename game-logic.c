#include <assert.h>
#include <cards.h>
#include <font.h>
#include <game-logic.h>
#include <image.h>
#include <keycodes.h>
#include <limits.h>
#include <nonstddef.h>
#include <ogg-vorbis.h>
#include <platform.h>
#include <regions.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


const char *const party_name[PARTY_COUNT] = {
    [RED] = "red",
    [BLUE] = "blue",
#ifdef HAVE_NEUTRAL
    [GRAY] = "gray",
#endif
};

typedef enum GamePhase {
    INITIALIZATION,
    PREPARATION,
    MAIN,
    GAME_OVER,

    GAME_PHASE_COUNT
} GamePhase;

typedef enum MainPhase {
    MAIN_WAITING_FOR_OTHER,

    MAIN_TRADE_IN_CARDS,
    MAIN_REINFORCEMENT,
    MAIN_BATTLE,
    MAIN_BATTLE_TRADE_IN_CARDS,
    MAIN_MOVEMENT,

    MAIN_PHASE_COUNT,

    MAIN_START = MAIN_TRADE_IN_CARDS
} MainPhase;


#define STATUS_X 1320
#define STATUS_PHASE_Y   10
#define STATUS_PHASE_H   40
#define STATUS_TODO_Y    60
#define STATUS_TODO_H    60
#define STATUS_INFO_Y   120
#define STATUS_INFO_H    40
#define STATUS_PROMPT_Y 160
#define STATUS_PROMPT_H  20
#define STATUS_HAND_Y   180
#define STATUS_HAND_H   460
#define STATUS_ERROR_Y  800
#define STATUS_ERROR_H   80


typedef struct LoadedImage {
    int w, h;
    uint32_t *d;
} LoadedImage;

typedef struct LoadedSound {
    int16_t *samples;
    int64_t frame_count;
    int frame_rate, channels;
} LoadedSound;


static int mouse_x, mouse_y;
static bool need_cursor_updates;

static uint32_t *fb;
static int fbw, fbh;
static size_t fb_stride;

static uint8_t *region_areas;

static uint32_t *bg_image;
static uint32_t *defeat_img, *victory_img;

static int army_img_w, army_img_h;
static uint32_t *army_img[PARTY_COUNT][100];

static RegionID ai_focused_region, focused_region;
static RegionID attacking_region, defending_region;
static RegionID origin_region, destination_region;
static int attacking_count, defending_count;
static bool ai_waiting_for_defending_count;

static LoadedImage region_focus_img, attacking_region_img, attacked_region_img;
static LoadedImage origin_region_img, destination_region_img, error_icon;
static int region_troops_max_w, region_troops_max_h;

static int headings_w, headings_h = STATUS_PHASE_H;
static uint32_t *game_phase_headings[GAME_PHASE_COUNT];
static uint32_t *main_phase_headings[MAIN_PHASE_COUNT];

static int dice_w, dice_h;
static uint32_t *dice_img[6];

static GamePhase game_phase = INITIALIZATION;
static MainPhase main_phase[PARTY_COUNT];

static LoadedSound battle_snd;
static LoadedSound beep1_snd;
static LoadedSound beep2_snd;
static LoadedSound capture_snd;
static LoadedSound movement_snd;
static LoadedSound notification_snd;
static LoadedSound reinforcements_snd;
static LoadedSound victory_snd;

// For HAVE_NEUTRAL: Every party places two of their own armies,
// (indices 0, 1), then a neutral army (index 2)
static int preparation_placement_index;

// A bad hack I don't even want to explain
static uint64_t ai_place_troops_stamp1[PARTY_COUNT];
static uint64_t ai_place_troops_stamp2[PARTY_COUNT];
static uint64_t ai_place_neutral_troops_stamp[PARTY_COUNT];

static int troops_to_place[PARTY_COUNT];

// for 42 regions (for simplicity, we just scale down, rounding up)
static const int initial_troops_reference[] = {
    0,  // 1 player is not allowed
    45, // no source for this
#ifdef HAVE_NEUTRAL
    40,
#else
    35,
#endif
    30,
    25,
};

_Static_assert(PARTY_COUNT > 1 &&
               PARTY_COUNT < ARRAY_SIZE(initial_troops_reference),
               "Invalid party count");

/* This is not scaled down.  Collecting three cards is comparatively
 * more difficult with fewer regions overall (and fewer initial
 * armies), so we can use the same numbers as in standard game. */
static const int trade_in_troops[] = {
    4,
    6,
    8,
    10,
    12,
    15,
};
static const int trade_in_troops_multiplier = 5;

static int trade_in_set_index;

enum IntegerPromptPurpose {
    NO_PROMPT,
    ATTACK_TROOPS_COUNT,
    CLAIM_TROOPS_COUNT,
    MOVE_TROOPS_COUNT,
    DEFENSE_TROOPS_COUNT,
};

static enum IntegerPromptPurpose integer_prompt;
static enum IntegerPromptPurpose integer_prompt_done;
static int integer_prompt_chrs = -1;
static int integer_prompt_value;

static bool party_defeated[PARTY_COUNT];

static bool draw_card_this_turn;
static int focused_card = -1;
static uint64_t ai_select_trade_in_timestamp = 0;
static uint64_t ai_do_trade_in_timestamp = (uint64_t)-1;

static uint32_t *card_design_img[CARD_DESIGN_COUNT + 1]; // 32x32
static uint32_t *card_bg_img, *card_hover_img, *card_selected_img; // 280x40

_Static_assert(CARD_WILDCARD == CARD_DESIGN_COUNT,
               "CARD_WILDCARD should be at index CARD_DESIGN_COUNT");


static void queue_sfx(const LoadedSound *sfx)
{
    platform_funcs.queue_audio_track(sfx->samples, sfx->frame_count,
                                     sfx->frame_rate, sfx->channels, NULL);
}

static void load_sfx(const char *fname, LoadedSound *sfx)
{
    if (!load_ogg_vorbis(fname, &sfx->samples, &sfx->frame_count,
                         &sfx->frame_rate, &sfx->channels))
    {
        abort();
    }
}

static void __attribute__((format(printf, 1, 6)))
    load_img(const char *fname_format, uint32_t **d, int *w, int *h, int s, ...)
{
    va_list ap;
    char fname[256];

    va_start(ap, s);
    vsnprintf(fname, sizeof(fname), fname_format, ap);
    va_end(ap);

    if (!load_image(fname, d, w, h, s)) {
        printf("Failed to load %s\n", fname);
        abort();
    }
}


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

    load_img("/bg.png", &bg_image, &fbw, &fbh, fb_stride);

    uint32_t *region_areas_img = NULL;
    load_img("/region-areas.png", &region_areas_img, &fbw, &fbh, 0);

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

    load_img("/error-icon.png", &error_icon.d, &error_icon.w, &error_icon.h, 0);

    load_img("/army-none.png", &army_img[0][0], &army_img_w, &army_img_h, 0);
    for (Party p = 1; p < PARTY_COUNT; p++) {
        army_img[p][0] = army_img[0][0];
    }

    for (int i = 1; i < 100; i++) {
        for (Party p = 0; p < PARTY_COUNT; p++) {
            load_img("/army-%s-%i.png", &army_img[p][i], &army_img_w,
                     &army_img_h, 0, party_name[p], i);
        }
    }

    load_img("/focus-region.png", &region_focus_img.d, &region_focus_img.w,
             &region_focus_img.h, 0);
    region_troops_max_w = MAX(region_troops_max_w, region_focus_img.w);
    region_troops_max_h = MAX(region_troops_max_h, region_focus_img.h);

    load_img("/attacking-region.png", &attacking_region_img.d,
             &attacking_region_img.w, &attacking_region_img.h, 0);
    region_troops_max_w = MAX(region_troops_max_w, attacking_region_img.w);
    region_troops_max_h = MAX(region_troops_max_h, attacking_region_img.h);

    load_img("/attacked-region.png", &attacked_region_img.d,
             &attacked_region_img.w, &attacked_region_img.h, 0);
    region_troops_max_w = MAX(region_troops_max_w, attacked_region_img.w);
    region_troops_max_h = MAX(region_troops_max_h, attacked_region_img.h);

    load_img("/origin-region.png", &origin_region_img.d, &origin_region_img.w,
             &origin_region_img.h, 0);
    region_troops_max_w = MAX(region_troops_max_w, origin_region_img.w);
    region_troops_max_h = MAX(region_troops_max_h, origin_region_img.h);

    load_img("/destination-region.png", &destination_region_img.d,
             &destination_region_img.w, &destination_region_img.h, 0);
    region_troops_max_w = MAX(region_troops_max_w, destination_region_img.w);
    region_troops_max_h = MAX(region_troops_max_h, destination_region_img.h);

    for (int i = 0; i < 6; i++) {
        load_img("/die-%i.png", &dice_img[i], &dice_w, &dice_h, 0, i + 1);
    }

    load_img("/defeat.png", &defeat_img, &fbw, &fbh, 0);
    load_img("/victory.png", &victory_img, &fbw, &fbh, 0);

    headings_w = fbw - STATUS_X;
    load_img("/preparation.png", &game_phase_headings[PREPARATION],
             &headings_w, &headings_h, 0);
    load_img("/game-over.png", &game_phase_headings[GAME_OVER],
             &headings_w, &headings_h, 0);
    load_img("/waiting-for-other.png",
             &main_phase_headings[MAIN_WAITING_FOR_OTHER],
             &headings_w, &headings_h, 0);
    load_img("/reinforcements.png", &main_phase_headings[MAIN_REINFORCEMENT],
             &headings_w, &headings_h, 0);
    load_img("/trade-in.png", &main_phase_headings[MAIN_TRADE_IN_CARDS],
             &headings_w, &headings_h, 0);
    load_img("/battle.png", &main_phase_headings[MAIN_BATTLE],
             &headings_w, &headings_h, 0);
    load_img("/movement.png", &main_phase_headings[MAIN_MOVEMENT],
             &headings_w, &headings_h, 0);

    main_phase_headings[MAIN_BATTLE_TRADE_IN_CARDS] =
        main_phase_headings[MAIN_TRADE_IN_CARDS];

    for (CardDesign d = 0; d <= CARD_WILDCARD; d++) {
        int card_design_w = 32, card_design_h = 32;
        load_img("/card-design-%i.png", &card_design_img[d], &card_design_w,
                 &card_design_h, 0, d);
    }

    int card_marker_w = fbw - STATUS_X, card_marker_h = 40;
    load_img("/card-bg.png", &card_bg_img, &card_marker_w, &card_marker_h, 0);
    load_img("/card-hover.png", &card_hover_img,
             &card_marker_w, &card_marker_h, 0);
    load_img("/card-selected.png", &card_selected_img,
             &card_marker_w, &card_marker_h, 0);

    load_sfx("/battle.ogg", &battle_snd);
    load_sfx("/beep1.ogg", &beep1_snd);
    load_sfx("/beep2.ogg", &beep2_snd);
    load_sfx("/capture.ogg", &capture_snd);
    load_sfx("/movement.ogg", &movement_snd);
    load_sfx("/notification.ogg", &notification_snd);
    load_sfx("/reinforcements.ogg", &reinforcements_snd);
    load_sfx("/victory.ogg", &victory_snd);
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

    clear_to_bg(xmin, ymin, xmax - xmin, ymax - ymin);

    for (RegionID i = 1; i < REGION_COUNT; i++) {
        ablitlmt(fb, army_img[regions[i].taken_by][MIN(regions[i].troops, 99)],
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

        if (i == defending_region) {
            ablitlmt(fb, attacked_region_img.d,
                     regions[i].troops_pos.x - attacked_region_img.w / 2,
                     regions[i].troops_pos.y - attacked_region_img.h / 2,
                     attacked_region_img.w, attacked_region_img.h,
                     fb_stride, attacked_region_img.w * sizeof(uint32_t),
                     xmin, ymin, xmax, ymax);
        }

        if (i == origin_region) {
            ablitlmt(fb, origin_region_img.d,
                     regions[i].troops_pos.x - origin_region_img.w / 2,
                     regions[i].troops_pos.y - origin_region_img.h / 2,
                     origin_region_img.w, origin_region_img.h,
                     fb_stride, origin_region_img.w * sizeof(uint32_t),
                     xmin, ymin, xmax, ymax);
        }

        if (i == destination_region) {
            ablitlmt(fb, destination_region_img.d,
                     regions[i].troops_pos.x - destination_region_img.w / 2,
                     regions[i].troops_pos.y - destination_region_img.h / 2,
                     destination_region_img.w, destination_region_img.h,
                     fb_stride, destination_region_img.w * sizeof(uint32_t),
                     xmin, ymin, xmax, ymax);
        }

        if (i == ai_focused_region) {
            ablitlmt(fb, region_focus_img.d,
                     regions[i].troops_pos.x - region_focus_img.w / 2,
                     regions[i].troops_pos.y - region_focus_img.h / 2,
                     region_focus_img.w, region_focus_img.h,
                     fb_stride, region_focus_img.w * sizeof(uint32_t),
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

    if (game_phase == GAME_OVER) {
        if (party_defeated[PLAYER]) {
            ablitlmt(fb, defeat_img, 0, 0, fbw, fbh, fb_stride,
                     fbw * sizeof(uint32_t), xmin, ymin, xmax, ymax);
        } else {
            ablitlmt(fb, victory_img, 0, 0, fbw, fbh, fb_stride,
                     fbw * sizeof(uint32_t), xmin, ymin, xmax, ymax);
        }
    }

    platform_funcs.fb_flush(xmin, ymin, xmax - xmin, ymax - ymin);
}


static void refresh_hand(void)
{
    Party p;
    for (p = 0; p < PARTY_COUNT; p++) {
        if (main_phase[p] != MAIN_WAITING_FOR_OTHER) {
            break;
        }
    }

    clear_to_bg(STATUS_X, STATUS_HAND_Y, fbw - STATUS_X, STATUS_HAND_H);

    if (p == PARTY_COUNT) {
        platform_funcs.fb_flush(STATUS_X, STATUS_HAND_Y, fbw - STATUS_X,
                                STATUS_HAND_H);
        return;
    }

    if (p == PLAYER) {
        font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_HAND_Y,
                       "Your cards:", 0);
    }

    // Hoo boy FIXME
    int card_offset = p == PLAYER ? 20 : 30;

    int y = STATUS_HAND_Y + card_offset;
    for (int i = 0; i < party_hand_size(p); i++) {
        const Card *c = party_hand_card(p, i);

        // You can have a maximum of eleven cards in hand (I think),
        // so this should be enough
        assert(y < STATUS_HAND_Y + STATUS_HAND_H);

        if (p == PLAYER || c->selected) {
            int w = fbw - STATUS_X;
            ablitlmt(fb, card_bg_img, STATUS_X, y, w, 40, fb_stride,
                     w * sizeof(uint32_t), STATUS_X, y, fbw, y + 40);
        }

        if (c->selected) {
            int w = fbw - STATUS_X;
            ablitlmt(fb, card_selected_img, STATUS_X, y, w, 40, fb_stride,
                     w * sizeof(uint32_t), STATUS_X, y, fbw, y + 40);
        }

        if (p == PLAYER && focused_card == i) {
            int w = fbw - STATUS_X;
            ablitlmt(fb, card_hover_img, STATUS_X, y, w, 40, fb_stride,
                     w * sizeof(uint32_t), STATUS_X, y, fbw, y + 40);
        }

        if (p == PLAYER || c->selected) {
            ablitlmt(fb, card_design_img[c->design], STATUS_X + 16, y + 4, 32, 32,
                     fb_stride, 32 * sizeof(uint32_t), STATUS_X, y, fbw, y + 40);

            font_draw_text(fb, fbw - 16, fbh, fb_stride, STATUS_X + 56, y + 14,
                           c->design == CARD_WILDCARD ? "(Wildcard)"
                                                      : regions[c->region].name,
                           0);

            y += 40;
        }
    }

    if (y > STATUS_HAND_Y + card_offset && p != PLAYER) {
        font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_HAND_Y,
                       "Cards traded in by your opponent:", 0);
    }

    platform_funcs.fb_flush(STATUS_X, STATUS_HAND_Y, fbw - STATUS_X,
                            STATUS_HAND_H);
}


static void clear_invalid_move(void);

static void important_message(const char *message, bool error)
{
    clear_invalid_move();

    puts(message);

    ablitlmt(fb, error_icon.d, STATUS_X, STATUS_ERROR_Y, error_icon.w,
             error_icon.h, fb_stride, error_icon.w * sizeof(uint32_t), 0, 0,
             fbw, fbh);

    font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X + error_icon.w + 10,
                   STATUS_ERROR_Y, message, 0x400000);

    platform_funcs.fb_flush(STATUS_X, STATUS_ERROR_Y,
                            fbw - STATUS_X, fbh - STATUS_ERROR_Y);

    // We might want a different notification sound, depending on
    // whether it is a user error or just an in-game notification
    (void)error;
    queue_sfx(&notification_snd);
}

static void invalid_move(const char *message)
{
    char full_msg[256];
    snprintf(full_msg, sizeof(full_msg), "Invalid move: %s", message);
    important_message(full_msg, true);
}


static void clear_invalid_move(void)
{
    clear_to_bg(STATUS_X, STATUS_ERROR_Y, fbw - STATUS_X, STATUS_ERROR_H);
    platform_funcs.fb_flush(STATUS_X, STATUS_ERROR_Y, fbw - STATUS_X,
                            STATUS_ERROR_H);
}


static bool has_any_card_set(Party p)
{
    int design_counts[CARD_DESIGN_COUNT] = { 0 };
    int wildcard_count = 0;

    for (int i = 0; i < party_hand_size(p); i++) {
        if (party_hand_card(p, i)->design == CARD_WILDCARD) {
            wildcard_count++;
        } else {
            design_counts[party_hand_card(p, i)->design]++;
        }
    }

    int different_designs = wildcard_count;
    bool has_single_set = false;

    for (CardDesign d = 0; d < CARD_DESIGN_COUNT; d++) {
        if (design_counts[d]) {
            different_designs++;
        }
        if (design_counts[d] + wildcard_count >= 3) {
            has_single_set = true;
        }
    }

    return has_single_set || different_designs >= CARD_DESIGN_COUNT;
}


static void switch_main_phase(Party p, MainPhase new_phase)
{
    switch (new_phase) {
        case MAIN_WAITING_FOR_OTHER: {
            if (p == PLAYER) {
                clear_to_bg(STATUS_X, 0, fbw - STATUS_X, fbh);
                ablitlmt(fb, main_phase_headings[new_phase], STATUS_X,
                         STATUS_PHASE_Y, headings_w, headings_h, fb_stride,
                         headings_w * sizeof(uint32_t), STATUS_X, STATUS_PHASE_Y,
                         fbw, STATUS_PHASE_Y + STATUS_PHASE_H);
                platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X, fbh);
            }

            if (game_phase == MAIN) {
                Party np = (p + 1) % PARTY_COUNT;
                for (;;) {
                    if (!party_defeated[np]
#ifdef HAVE_NEUTRAL
                        && np != NEUTRAL
#endif
                       )
                    {
                        switch_main_phase(np, MAIN_START);
                        break;
                    }

                     np = (np + 1) % PARTY_COUNT;
                }

                if (p == np) {
                    // Do not switch to MAIN_WAITING_FOR_OTHER
                    return;
                }
            }
            break;
        }

        case MAIN_TRADE_IN_CARDS: {
            if (!has_any_card_set(p)) {
                switch_main_phase(p, MAIN_REINFORCEMENT);
                return;
            }
            if (p == PLAYER) {
                clear_to_bg(STATUS_X, 0, fbw - STATUS_X, fbh);
                ablitlmt(fb, main_phase_headings[new_phase], STATUS_X,
                         STATUS_PHASE_Y, headings_w, headings_h, fb_stride,
                         headings_w * sizeof(uint32_t), STATUS_X, STATUS_PHASE_Y,
                         fbw, STATUS_PHASE_Y + STATUS_PHASE_H);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                               "Choose cards to trade in for extra armies.", 0);
                if (party_hand_size(p) < 5) {
                    font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X,
                                   STATUS_INFO_Y,
                                   "Press enter to confirm, or the space bar "
                                   "to skip.", 0);
                }
                platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X, fbh);
            }
            break;
        }

        case MAIN_BATTLE_TRADE_IN_CARDS: {
            assert(has_any_card_set(p));
            abort(); // FIXME
        }

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
            troops_to_place[p] += MAX(2, regions_taken / 3);
            for (ContinentID c = 1; c < CONTINENT_COUNT; c++) {
                if (continents_taken & (1u << c)) {
                    troops_to_place[p] += continent_bonuses[c];
                }
            }

            if (p == PLAYER) {
                char buf[64];
                snprintf(buf, sizeof(buf), "You have %i %s remaining.",
                         troops_to_place[p],
                         troops_to_place[p] == 1 ? "army" : "armies");

                clear_to_bg(STATUS_X, 0, fbw - STATUS_X, fbh);
                ablitlmt(fb, main_phase_headings[new_phase], STATUS_X,
                         STATUS_PHASE_Y, headings_w, headings_h, fb_stride,
                         headings_w * sizeof(uint32_t), STATUS_X, STATUS_PHASE_Y,
                         fbw, STATUS_PHASE_Y + STATUS_PHASE_H);
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
                ablitlmt(fb, main_phase_headings[new_phase], STATUS_X,
                         STATUS_PHASE_Y, headings_w, headings_h, fb_stride,
                         headings_w * sizeof(uint32_t), STATUS_X, STATUS_PHASE_Y,
                         fbw, STATUS_PHASE_Y + STATUS_PHASE_H);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                               "Choose a region to attack from.", 0);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_INFO_Y,
                               "Press the space bar to end the battle phase.",
                               0);
                platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X, fbh);
            }
            break;
        }

        case MAIN_MOVEMENT: {
            if (draw_card_this_turn) {
                draw_card_from_stack(p);
                refresh_hand();
                draw_card_this_turn = false;
            }
            if (p == PLAYER) {
                clear_to_bg(STATUS_X, 0, fbw - STATUS_X, fbh);
                ablitlmt(fb, main_phase_headings[new_phase], STATUS_X,
                         STATUS_PHASE_Y, headings_w, headings_h, fb_stride,
                         headings_w * sizeof(uint32_t), STATUS_X, STATUS_PHASE_Y,
                         fbw, STATUS_PHASE_Y + STATUS_PHASE_H);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                               "Choose one region to move troops from.", 0);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_INFO_Y,
                               "Press the space bar to skip.", 0);
                platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X, fbh);
            }
            break;
        }

        default:
            abort();
    }

    main_phase[p] = new_phase;
    refresh_hand();
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

#ifdef HAVE_NEUTRAL
            // With a neutral party, you cannot choose where to put
            // your troops -- it's chosen at random.

            int region_stack[REGION_COUNT - 1];
            for (RegionID r = 1; r < REGION_COUNT; r++) {
                region_stack[r - 1] = r;
            }
            shuffle(region_stack, REGION_COUNT - 1, sizeof(region_stack[0]));

            int region_stack_i = 0;

            while (region_stack_i < REGION_COUNT - 1) {
                int regions_left = REGION_COUNT - 1 - region_stack_i;

                if (regions_left < PARTY_COUNT) {
                    // OK, what to do?
                    if (regions_left == 1) {
                        // OK, the neutral party gets it
                        RegionID r = region_stack[region_stack_i++];
                        troops_to_place[NEUTRAL]--;
                        regions[r].taken_by = NEUTRAL;
                        regions[r].troops = 1;
                        continue;
                    } else if (regions_left == PARTY_COUNT - 1) {
                        // OK, all but the neutral party get one
                        for (Party p = 0; p < PARTY_COUNT; p++) {
                            if (p == NEUTRAL) {
                                continue;
                            }

                            RegionID r = region_stack[region_stack_i++];
                            troops_to_place[p]--;
                            regions[r].taken_by = p;
                            regions[r].troops = 1;
                        }
                        continue;
                    }

                    // Otherwise, no idea.  Just hand them out in
                    // order.
                }

                for (Party p = 0;
                     p < PARTY_COUNT && region_stack_i < REGION_COUNT - 1;
                     p++)
                {
                    RegionID r = region_stack[region_stack_i++];
                    troops_to_place[p]--;
                    regions[r].taken_by = p;
                    regions[r].troops = 1;
                }
            }

            /* Now add neutral troops so every party gets the same
             * number to place
             * (This is not in standard risk, because it is not
             * necessary there -- the number of regions and troops is
             * chosen such that this is the case anyway.) */
            if (troops_to_place[NEUTRAL] % (PARTY_COUNT - 1)) {
                troops_to_place[NEUTRAL] += PARTY_COUNT - 1 -
                    troops_to_place[NEUTRAL] % (PARTY_COUNT - 1);
            }
#endif

            clear_to_bg(STATUS_X, 0, fbw - STATUS_X, fbh);

            ablitlmt(fb, game_phase_headings[new_phase], STATUS_X,
                     STATUS_PHASE_Y, headings_w, headings_h, fb_stride,
                     headings_w * sizeof(uint32_t), STATUS_X, STATUS_PHASE_Y,
                     fbw, STATUS_PHASE_Y + STATUS_PHASE_H);

            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
#ifdef HAVE_NEUTRAL
                           "Reinforce your regions by placing two additional "
                           "armies per turn.",
#else
                           "Claim regions by placing troops in them.",
#endif
                           0);

#ifdef HAVE_NEUTRAL
            char buf[64];
            snprintf(buf, sizeof(buf), "You have %i %s remaining.",
                     troops_to_place[PLAYER],
                     troops_to_place[PLAYER] == 1 ? "army" : "armies");
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_INFO_Y,
                           buf, 0);
#endif

            platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X, fbh);

            break;
        }

        case MAIN: {
            setup_card_stack();
            for (Party p = 0; p < PARTY_COUNT; p++) {
                switch_main_phase(p, MAIN_WAITING_FOR_OTHER);
            }
            switch_main_phase(0, MAIN_START);
            break;
        }

        case GAME_OVER: {
            for (Party p = 0; p < PARTY_COUNT; p++) {
                main_phase[p] = MAIN_WAITING_FOR_OTHER;
            }

            ai_focused_region = focused_region = NULL_REGION;
            attacking_region = defending_region = NULL_REGION;
            origin_region = destination_region = NULL_REGION;

            clear_to_bg(STATUS_X, 0, fbw - STATUS_X, fbh);

            ablitlmt(fb, game_phase_headings[new_phase], STATUS_X,
                     STATUS_PHASE_Y, headings_w, headings_h, fb_stride,
                     headings_w * sizeof(uint32_t), STATUS_X, STATUS_PHASE_Y,
                     fbw, STATUS_PHASE_Y + STATUS_PHASE_H);

            if (party_defeated[PLAYER]) {
                ablitlmt(fb, defeat_img, 0, 0, fbw, fbh, fb_stride,
                         fbw * sizeof(uint32_t), STATUS_X, 0, fbw, fbh);
            } else {
                ablitlmt(fb, victory_img, 0, 0, fbw, fbh, fb_stride,
                             fbw * sizeof(uint32_t), STATUS_X, 0, fbw, fbh);
                queue_sfx(&victory_snd);
            }

            platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X, fbh);
            break;
        }

        default:
            abort();
    }

    game_phase = new_phase;

    if (game_phase == GAME_OVER) {
        refresh(0, 0, STATUS_X, fbh);
    }
}


static int ai_choose_defense_count(void)
{
    return MIN(regions[defending_region].troops, 2);
}


static void check_win_condition(void)
{
    int opponents_active = 0;

    for (Party p = 0; p < PARTY_COUNT; p++) {
        if (party_defeated[p]) {
            continue;
        }

        bool has_region = false;
        for (RegionID r = 1; r < REGION_COUNT; r++) {
            if (regions[r].troops && regions[r].taken_by == p) {
                has_region = true;
                break;
            }
        }

        if (has_region) {
            if (p != PLAYER
#ifdef HAVE_NEUTRAL
                && p != NEUTRAL
#endif
               )
            {
                opponents_active++;
            }
        } else {
            party_defeated[p] = true;
        }
    }

    if (party_defeated[PLAYER] || !opponents_active) {
        switch_game_phase(GAME_OVER);
    }
}


static int compare_ints_rev(const void *x, const void *y)
{
    return *(const int *)y - *(const int *)x;
}

// Battles and returns true if the defending region is empty after
static bool battle(void)
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
             "Attacker lost %i %s, defender lost %i %s.",
             attacking_losses, attacking_losses == 1 ? "army" : "armies",
             defending_losses, defending_losses == 1 ? "army" : "armies");
    font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_ERROR_Y,
                   casualties, 0);

    regions[attacking_region].troops -= attacking_losses;
    regions[defending_region].troops -= defending_losses;

    platform_funcs.fb_flush(STATUS_X, start_y, fbw - STATUS_X, fbh - start_y);

    if (regions[defending_region].troops) {
        queue_sfx(&battle_snd);
        return false;
    } else {
        int troops_moved = attacking_count - attacking_losses;
        Party attacker = regions[attacking_region].taken_by;
        Party defender = regions[defending_region].taken_by;

        regions[defending_region].taken_by = attacker;
        regions[attacking_region].troops -= troops_moved;
        regions[defending_region].troops += troops_moved;

        queue_sfx(&capture_snd);

        assert(!party_defeated[defender]);
        check_win_condition();

        if (party_defeated[defender] && game_phase != GAME_OVER) {
            join_party_hands(attacker, defender);
            refresh_hand();
            if (party_hand_size(attacker) >= 6) {
                switch_main_phase(attacker, MAIN_BATTLE_TRADE_IN_CARDS);
            }
        }
        draw_card_this_turn = true;

        return true;
    }
}


static bool trade_in_cards(Party p, Card cards[3])
{
    int design_counts[CARD_DESIGN_COUNT + 1] = { 0 };

    for (int i = 0; i < 3; i++) {
        design_counts[cards[i].design]++;
    }

    int different_designs = design_counts[CARD_WILDCARD];
    bool has_single_set = false;
    for (CardDesign d = 0; d < CARD_DESIGN_COUNT; d++) {
        different_designs += design_counts[d] > 0;
        has_single_set |= design_counts[d] + design_counts[CARD_WILDCARD] >=
                          CARD_DESIGN_COUNT;
    }

    if (different_designs < CARD_DESIGN_COUNT && !has_single_set) {
        return false;
    }

    /* XXX: The rules are inconclusive on this.  Some say you do not
     * get any bonus for matching regions that you have occupied.
     * Others say you get two armies, but only once per turn.  It is
     * unclear, however, how the matching region is chosen in this
     * case (probably by the player).  Finally, at least the (2015)
     * German manual says you get two extra armies if a card matches a
     * region taken by you, and nothing about a maximum, so this
     * sounds like you simply get two armies per matching region.
     *
     * I like having a bit of a strategic aspect here, so I want to
     * include extra armies.  I find it a bit boring to only hand out
     * two extra armies, because this will usually be a 1v1 game on a
     * small map, so you are very likely to just always have some card
     * that matches a region you own.  But having all three match?
     * That is more interesting.
     *
     * Sure, this will empower the offense even more (because the more
     * regions you have, the better), which I do not usually like, but
     * this (advent calendar) version of the game is supposed to be
     * quick, so hey, that's actually good.
     */
    bool region_match = false;
    for (int i = 0; i < 3; i++) {
        if (cards[i].region && regions[cards[i].region].taken_by == p) {
            // No empty regions during regular gameplay
            assert(regions[cards[i].region].troops);
            regions[cards[i].region].troops += 2;
            region_match = true;
        }
    }

    if (trade_in_set_index < (int)ARRAY_SIZE(trade_in_troops)) {
        troops_to_place[p] += trade_in_troops[trade_in_set_index++];
    } else {
        troops_to_place[p] += trade_in_troops[ARRAY_SIZE(trade_in_troops) - 1]
                            + (++trade_in_set_index -
                               ARRAY_SIZE(trade_in_troops)) *
                              trade_in_troops_multiplier;
    }

    if (region_match) {
        refresh(0, 0, STATUS_X, fbh);
        queue_sfx(&reinforcements_snd);
    } else {
        queue_sfx(&beep2_snd);
    }

    return true;
}


static bool do_friendly_connection(Party party, RegionID origin,
                                   RegionID destination,
                                   bool region_seen[REGION_COUNT])
{
    if (origin == destination) {
        return true;
    }
    if (region_seen[origin] || regions[origin].taken_by != party) {
        return false;
    }
    region_seen[origin] = true;

    for (int i = 0; i < regions[origin].neighbor_count; i++) {
        if (do_friendly_connection(party, regions[origin].neighbors[i],
                                   destination, region_seen))
        {
            return true;
        }
    }
    return false;
}

static bool friendly_connection(RegionID origin, RegionID destination)
{
    Party party = regions[origin].taken_by;
    bool region_seen[REGION_COUNT] = { false };

    return do_friendly_connection(party, origin, destination, region_seen);
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


// TODO: This just has no strategy
static bool get_ai_battle_params(Party p, RegionID *attacking,
                                 RegionID *attacked, int *attack_count)
{
    int best_score = INT_MIN;

    for (RegionID r = 1; r < REGION_COUNT; r++) {
        if (regions[r].taken_by != p || regions[r].troops <= 1) {
            continue;
        }

        for (int i = 0; i < regions[r].neighbor_count; i++) {
            RegionID d = regions[r].neighbors[i];
            if (regions[d].taken_by == p) {
                continue;
            }

            bool last_in_continent = true;
            int ric = 1; // regions in continent
            int ricobto = 0; // regions in continent owned by that opponent
            ContinentID c = regions[d].continent;
            for (RegionID cr = 1; cr < REGION_COUNT; cr++) {
                if (cr == d || regions[cr].continent != c) {
                    continue;
                }

                ric++;

                if (regions[cr].taken_by != p) {
                    last_in_continent = false;
                }
                if (regions[cr].taken_by == regions[d].taken_by) {
                    ricobto++;
                }
            }

            int diff = regions[r].troops - regions[d].troops;

            int score = diff;
            if (last_in_continent) {
                if (regions[r].troops >= 4) {
                    // Sufficient troops to attack with 3
                    score += 5;
                } else if (diff > 0) {
                    // Risky, but let's try
                    score += 2;
                }
            }

            // Try to keep others from taking continents
            int x = MAX(0, 10 * (2 * ricobto - ric) / ric);
            if (regions[r].troops >= 4) {
                score += x;
            } else if (diff > 0) {
                score += 2 * x / 3;
            } else {
                score += x / 3;
            }

            if (score > 0) {
                // If attacking makes sense and we have a card with
                // this region in hand, let's (maybe) go for it
                for (int j = 0; j < party_hand_size(p); j++) {
                    if (party_hand_card(p, j)->region == d) {
                        score += 2;
                        break;
                    }
                }
            }

            score -= rand() % 3;

            if (score > best_score) {
                best_score = score;
                *attacking = r;
                *attacked = d;
                *attack_count = MIN(3, regions[r].troops - 1);
            }
        }
    }

    if (best_score <= -(rand() % 3)) {
        *attacking = NULL_REGION;
        *attacked = NULL_REGION;
        *attack_count = 0;
        return false;
    } else {
        return true;
    }
}

static void do_ai_find_connected_dest(Party p, RegionID o, RegionID *d,
                                      int *best_score,
                                      bool region_seen[REGION_COUNT])
{
    if (region_seen[o]) {
        return;
    }
    region_seen[o] = true;

    if (regions[o].taken_by != p) {
        return;
    }

    int enemy_count = 0;
    for (int i = 0; i < regions[o].neighbor_count; i++) {
        RegionID n = regions[o].neighbors[i];
        if (regions[n].taken_by != p) {
            enemy_count++;
        }

        do_ai_find_connected_dest(p, n, d, best_score, region_seen);
    }

    if (enemy_count && enemy_count > *best_score) {
        *best_score = enemy_count;
        *d = o;
    }
}

static void ai_find_connected_dest(Party p, RegionID o, RegionID *d,
                                   int *best_score)
{
    bool region_seen[REGION_COUNT] = { false };
    do_ai_find_connected_dest(p, o, d, best_score, region_seen);
}

// FIXME (as in, much TODO)
static void ai_move(Party p)
{
    static bool movement_started;
    static bool tictoc = false;

    if (tictoc) {
        tictoc = false;

        if (game_phase == PREPARATION ||
            main_phase[p] == MAIN_REINFORCEMENT)
        {
            regions[ai_focused_region].troops++;
            troops_to_place[p]--;
            queue_sfx(&reinforcements_snd);
        } else if (main_phase[p] == MAIN_BATTLE) {
            // Wait until this is set
            if (!defending_count) {
                tictoc = true;
                return;
            }

            if (battle()) {
                if (game_phase == GAME_OVER) {
                    return;
                }

                bool has_enemy_neighbors = false;
                for (int i = 0; i < regions[attacking_region].neighbor_count; i++) {
                    RegionID n = regions[attacking_region].neighbors[i];
                    if (regions[n].taken_by != p
#ifdef HAVE_NEUTRAL
                        && regions[n].taken_by != NEUTRAL
#endif
                       )
                    {
                        has_enemy_neighbors = true;
                        break;
                    }
                }

                int move = has_enemy_neighbors ?
                    (regions[attacking_region].troops -
                     regions[defending_region].troops) / 2 :
                    regions[attacking_region].troops - 1;

                if (move > 0) {
                    regions[attacking_region].troops -= move;
                    regions[defending_region].troops += move;
                }
            }
        } else if (main_phase[p] == MAIN_MOVEMENT) {
            if (origin_region) {
                int move = regions[origin_region].troops - 1;
                regions[origin_region].troops -= move;
                regions[destination_region].troops += move;

                queue_sfx(&movement_snd);
            }
        }

        return;
    }
    tictoc = true;

    ai_focused_region = NULL_REGION;
    attacking_region = defending_region = NULL_REGION;
    origin_region = destination_region = NULL_REGION;

    if (game_phase == PREPARATION ||
        main_phase[p] == MAIN_REINFORCEMENT)
    {
        if (!troops_to_place[p]) {
            if (game_phase == MAIN) {
                switch_main_phase(p, MAIN_BATTLE);
            }
            tictoc = false;
            return;
        }

        int region_scores[REGION_COUNT] = { 0 };

        // Try to reinforce regions around the last region of a
        // continent not yet owned; and to reinforce regions around
        // continents fully owned by a single opponent
        for (ContinentID c = 1; c < CONTINENT_COUNT; c++) {
            int cont_regions_not_owned = 0;
            Party opponent = PARTY_COUNT;
            bool cont_owned_by_single_opponent = true;
            RegionID not_owned_region = NULL_REGION;

            for (RegionID r = 1; r < REGION_COUNT; r++) {
                if (regions[r].continent != c) {
                    continue;
                }

                if (opponent == PARTY_COUNT) {
                    opponent = regions[r].taken_by;
                }

                if (regions[r].taken_by != p) {
                    cont_regions_not_owned++;
                    not_owned_region = r;
                }
                if (regions[r].taken_by != opponent) {
                    cont_owned_by_single_opponent = false;
                }
            }

#ifdef HAVE_NEUTRAL
            if (opponent == NEUTRAL) {
                // Ignore continents owned by the neutral party
                cont_owned_by_single_opponent = false;
            }
#endif

            if (cont_regions_not_owned == 1) {
                for (int i = 0; i < regions[not_owned_region].neighbor_count;
                     i++)
                {
                    RegionID n = regions[not_owned_region].neighbors[i];
                    if (regions[n].taken_by != p) {
                        continue;
                    }

                    region_scores[n] += 20;
                }
            } else if (cont_owned_by_single_opponent) {
                for (RegionID r = 1; r < REGION_COUNT; r++) {
                    if (regions[r].continent != c) {
                        continue;
                    }

                    for (int i = 0; i < regions[r].neighbor_count; i++) {
                        RegionID n = regions[r].neighbors[i];
                        if (regions[n].taken_by != p) {
                            continue;
                        }

                        region_scores[n] += 5;
                    }
                }
            }
        }

        // Now hand additional score to regions that are around (weak)
        // enemy regions (and need reinforcements)
        for (RegionID r = 1; r < REGION_COUNT; r++) {
            if (regions[r].taken_by != p) {
                continue;
            }

            bool no_enemy_neighbors = true;
            for (int i = 0; i < regions[r].neighbor_count; i++) {
                RegionID n = regions[r].neighbors[i];
                if (regions[n].taken_by == p) {
                    continue;
                }
                no_enemy_neighbors = false;

                bool is_neutral = false;
#ifdef HAVE_NEUTRAL
                is_neutral = regions[n].taken_by == NEUTRAL;
#endif

                int diff = regions[r].troops - regions[n].troops;
                if (is_neutral) {
                    if (diff >= -2 && diff < 1) {
                        // Give a bit of incentive
                        region_scores[r] += 2;
                    }
                } else {
                    if (diff < -troops_to_place[p]) {
                        // All is lost
                        region_scores[r] -= 20;
                    } else if (diff < 0) {
                        region_scores[r] += -diff + 2;
                    } else {
                        region_scores[r] += 1;
                    }
                }

                for (int j = 0; j < party_hand_size(p); j++) {
                    if (party_hand_card(p, j)->region == r) {
                        // Oh, this might be useful
                        region_scores[r] += 3;
                        break;
                    }
                }
            }

            if (no_enemy_neighbors) {
                region_scores[r] -= 100;
            }
        }

        int best_score = INT_MIN;
        RegionID best_region = NULL_REGION;
        for (RegionID r = 1; r < REGION_COUNT; r++) {
            if (regions[r].taken_by == p && region_scores[r] > best_score) {
                best_score = region_scores[r];
                best_region = r;
            }
        }

        assert(best_region);
        ai_focused_region = best_region;
    } else if (main_phase[p] == MAIN_BATTLE) {
        if (!get_ai_battle_params(p, &attacking_region, &defending_region,
                                  &attacking_count))
        {
            switch_main_phase(p, MAIN_MOVEMENT);
            tictoc = false;
            movement_started = true;
            return;
        }

        assert(attacking_region && defending_region && attacking_count);
        assert(regions[attacking_region].taken_by == p);
        assert(regions[defending_region].taken_by != p);

        if (regions[defending_region].taken_by == PLAYER
#ifdef HAVE_NEUTRAL
            || regions[defending_region].taken_by == NEUTRAL
#endif
           )
        {
            defending_count = 0; // to be chosen by the user
            ai_waiting_for_defending_count = true;
        } else {
            defending_count = ai_choose_defense_count();
        }

        queue_sfx(&beep1_snd);
    } else if (main_phase[p] == MAIN_MOVEMENT) {
        if (!movement_started) {
            tictoc = false;
            switch_main_phase(p, MAIN_WAITING_FOR_OTHER);
            return;
        }
        movement_started = false;

        int best_score = INT_MIN;
        for (RegionID r = 1; r < REGION_COUNT; r++) {
            if (regions[r].taken_by != p || regions[r].troops <= 1) {
                continue;
            }

            bool has_enemy_neighbors = false;
            for (int i = 0; i < regions[r].neighbor_count; i++) {
                if (regions[regions[r].neighbors[i]].taken_by != p) {
                    has_enemy_neighbors = true;
                    break;
                }
            }

            if (has_enemy_neighbors) {
                continue;
            }

            RegionID d = NULL_REGION;
            int new_score = best_score;
            ai_find_connected_dest(p, r, &d, &new_score);

            if (new_score > best_score) {
                best_score = new_score;
                assert(friendly_connection(r, d));
                origin_region = r;
                destination_region = d;
            }
        }

        if (best_score == INT_MIN) {
            tictoc = false;
            switch_main_phase(p, MAIN_WAITING_FOR_OTHER);
            return;
        }

        queue_sfx(&beep1_snd);
    }
}


#ifdef HAVE_NEUTRAL
static void ai_place_neutral_troops(Party p)
{
    if (!troops_to_place[NEUTRAL]) {
        return;
    }

    int best_score = INT_MIN;
    ai_focused_region = NULL_REGION;

    for (RegionID r = 1; r < REGION_COUNT; r++) {
        if (regions[r].taken_by != NEUTRAL) {
            continue;
        }

        int score = 0;
        for (int i = 0; i < regions[r].neighbor_count; i++) {
            RegionID n = regions[r].neighbors[i];

            if (regions[n].taken_by == p) {
                score -= regions[n].troops;
            } else if (regions[n].taken_by != NEUTRAL) {
                score += regions[n].troops;
            }
        }

        if (score > best_score) {
            best_score = score;
            ai_focused_region = r;
        }
    }

    assert(ai_focused_region);
    regions[ai_focused_region].troops++;
    troops_to_place[NEUTRAL]--;

    queue_sfx(&reinforcements_snd);
}
#endif


static void ai_place_troops(Party p)
{
    if (!troops_to_place[p]) {
        return;
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
        // TODO: This is just random

        int empty_region_index = rand() % empty_region_count;

        for (RegionID r = 1; r < REGION_COUNT; r++) {
            if (!regions[r].troops && !empty_region_index--) {
                regions[r].taken_by = p;
                regions[r].troops = 1;
                troops_to_place[p]--;

                queue_sfx(&reinforcements_snd);

                return;
            }
        }
    } else if (my_region_count) {
        ai_move(p); // tic
        ai_move(p); // toc
    }
}


static bool ai_select_trade_in_cards(Party p)
{
    int design_counts[CARD_DESIGN_COUNT] = { 0 };
    int regions_taken[CARD_DESIGN_COUNT + 1] = { 0 };
    int wildcard_count = 0;

    for (int i = 0; i < party_hand_size(p); i++) {
        const Card *c = party_hand_card(p, i);
        if (c->design == CARD_WILDCARD) {
            wildcard_count++;
        } else {
            design_counts[c->design]++;
            if (regions[c->region].taken_by == p) {
                regions_taken[c->design]++;
            }
        }
    }

    // See the XXX not in trade_in_cards() on why we count regions
    // the way it's done here
    int max_regions_taken = -1;
    CardDesign max_regions_taken_d = CARD_DESIGN_COUNT;

    int rainbow_regions_taken = 0;
    int rainbow_wildcard_used = 0;

    for (CardDesign d = 0; d < CARD_DESIGN_COUNT; d++) {
        if (design_counts[d] + wildcard_count >= 3) {
            if (regions_taken[d] > max_regions_taken) {
                max_regions_taken = regions_taken[d];
                max_regions_taken_d = d;
            }
        }

        if (rainbow_regions_taken >= 0) {
            if (design_counts[d]) {
                if (regions_taken[d]) {
                    rainbow_regions_taken++;
                }
            } else if (rainbow_wildcard_used < wildcard_count) {
                rainbow_wildcard_used++;
            } else {
                rainbow_regions_taken = -1;
            }
        }
    }

    int regions_owned_by_this_player = 0;
    for (RegionID r = 1; r < REGION_COUNT; r++) {
        if (regions[r].taken_by == p) {
            regions_owned_by_this_player++;
        }
    }

    int min_regions;
    if (regions_owned_by_this_player < REGION_COUNT / 2) {
        // Looks bad, let's get those extra armies right away
        min_regions = 0;
    } else {
        // No need to rush, wait until we can get more from our cards
        min_regions = 1;
    }

    if (rainbow_regions_taken > max_regions_taken) {
        if (main_phase[p] == MAIN_TRADE_IN_CARDS &&
            rainbow_regions_taken < min_regions && party_hand_size(p) <= 4)
        {
            return false;
        }

        rainbow_wildcard_used = 0;
        for (CardDesign d = 0; d < CARD_DESIGN_COUNT; d++) {
            if (design_counts[d]) {
                if (regions_taken[d]) {
                    for (int i = 0; i < party_hand_size(p); i++) {
                        const Card *c = party_hand_card(p, i);
                        if (c->design == d && regions[c->region].taken_by == p)
                        {
                            party_hand_select_card(p, i);
                            break;
                        }
                    }
                } else {
                    for (int i = 0; i < party_hand_size(p); i++) {
                        const Card *c = party_hand_card(p, i);
                        if (c->design == d) {
                            party_hand_select_card(p, i);
                            break;
                        }
                    }
                }
            } else {
                assert(rainbow_wildcard_used++ < wildcard_count);
                for (int i = 0; i < party_hand_size(p); i++) {
                    const Card *c = party_hand_card(p, i);
                    if (c->design == CARD_WILDCARD) {
                        party_hand_select_card(p, i);
                        break;
                    }
                }
            }
        }
    } else {
        if (max_regions_taken < 0) {
            assert(party_hand_size(p) <= 4);
            return false;
        }

        if (main_phase[p] == MAIN_TRADE_IN_CARDS &&
            max_regions_taken < min_regions && party_hand_size(p) <= 4)
        {
            return false;
        }

        int cards_i = 0;
        for (int i = 0; i < party_hand_size(p) && cards_i < 3; i++) {
            const Card *c = party_hand_card(p, i);
            if (c->design == max_regions_taken_d &&
                regions[c->region].taken_by == p)
            {
                party_hand_select_card(p, i);
            }
        }

        for (int i = 0; i < party_hand_size(p) && cards_i < 3; i++) {
            const Card *c = party_hand_card(p, i);
            if (c->design == max_regions_taken_d) {
                party_hand_select_card(p, i);
            }
        }

        for (int i = 0; i < party_hand_size(p) && cards_i < 3; i++) {
            const Card *c = party_hand_card(p, i);
            if (c->design == CARD_WILDCARD) {
                party_hand_select_card(p, i);
            }
        }
    }

    return true;
}


static bool trade_in_selected_cards(Party p)
{
    int cards_i = 0;
    Card cards[3];
    memset(cards, 0, sizeof(cards));

    for (int i = 0; i < party_hand_size(p); i++) {
        const Card *c = party_hand_card(p, i);
        if (c->selected) {
            if (cards_i >= 3) {
                return false;
            }
            cards[cards_i++] = *c;
        }
    }
    if (cards_i != 3) {
        return false;
    }

    if (!trade_in_cards(p, cards)) {
        return false;
    }

    for (int i = 0; i < party_hand_size(p); i++) {
        const Card *c = party_hand_card(p, i);
        if (c->selected) {
            party_hand_drop_card(p, i--);
        }
    }

    return true;
}


void handle_game(void)
{
    int refresh_xmin = fbw, refresh_xmax = 0;
    int refresh_ymin = fbh, refresh_ymax = 0;

    if (game_phase == INITIALIZATION) {
        REFRESH_INCLUDE(0, 0, fbw, fbh);

        switch_game_phase(PREPARATION);
    }

    int key = -1, button = -1, key_pressed = -1;
    bool has_button, key_up, button_up;
    bool lbuttondown = false;

    bool got_key_event, got_pointing_event;

    got_key_event = platform_funcs.get_keyboard_event(&key, &key_up);
    got_pointing_event = platform_funcs.get_pointing_event(&mouse_x, &mouse_y,
                                                           &has_button, &button,
                                                           &button_up);

    if (got_pointing_event && need_cursor_updates) {
        platform_funcs.move_cursor(mouse_x, mouse_y);
    }

    if (game_phase == GAME_OVER) {
        return;
    }

    Party active_party;
    if (game_phase == MAIN) {
        for (active_party = 0; active_party < PARTY_COUNT; active_party++) {
            if (main_phase[active_party] != MAIN_WAITING_FOR_OTHER) {
                break;
            }
        }
        assert(active_party < PARTY_COUNT);
    } else {
        active_party = PARTY_COUNT;
    }

    if (got_key_event && !key_up) {
        key_pressed = key;
        clear_invalid_move();
    }

    if (got_pointing_event) {
        if (has_button && !button_up) {
            clear_invalid_move();
        }

        // .get_pointing_event() returns coordinates limited by our
        // previous call to .limitpointing_device()
        RegionID new_focused_region = region_areas[mouse_y * fbw + mouse_x];

        if (new_focused_region != focused_region) {
            if (focused_region) {
                REFRESH_INCLUDE_REGION_TROOPS(focused_region);
            }
            if (new_focused_region) {
                REFRESH_INCLUDE_REGION_TROOPS(new_focused_region);
            }
            focused_region = new_focused_region;

            if (game_phase == PREPARATION && ai_focused_region) {
                // FIXME -- I'm just too lazy to add another timer to
                // clean up the AI focus here
                REFRESH_INCLUDE_REGION_TROOPS(ai_focused_region);
                ai_focused_region = NULL_REGION;
            }
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

        if (key_pressed >= KEY_1 && key_pressed <= KEY_0 &&
            integer_prompt_value < 100)
        {
            int num = key_pressed == KEY_0 ? 0 : key_pressed - KEY_1 + 1;
            integer_prompt_value = (integer_prompt_value * 10) + num;
            integer_prompt_chrs++;
            if (!integer_prompt_value) {
                // Result of the stupid design
                integer_prompt_chrs = 1;
            }
            update = true;
        } else if (key_pressed == KEY_BACKSPACE && integer_prompt_chrs) {
            integer_prompt_value /= 10;
            integer_prompt_chrs--;
            update = true;
        } else if (key_pressed == KEY_ENTER) {
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

    if (main_phase[PLAYER] == MAIN_TRADE_IN_CARDS ||
        main_phase[PLAYER] == MAIN_BATTLE_TRADE_IN_CARDS)
    {
        if (main_phase[PLAYER] == MAIN_TRADE_IN_CARDS &&
            party_hand_size(PLAYER) < 5 &&
            key_pressed == KEY_SPACE)
        {
            for (int i = 0; i < party_hand_size(PLAYER); i++) {
                party_hand_deselect_card(PLAYER, i);
            }
            focused_card = -1;
            switch_main_phase(PLAYER, MAIN_REINFORCEMENT);
            goto post_logic;
        } else if (main_phase[PLAYER] == MAIN_BATTLE_TRADE_IN_CARDS &&
                   party_hand_size(PLAYER) < 5)
        {
            for (int i = 0; i < party_hand_size(PLAYER); i++) {
                party_hand_deselect_card(PLAYER, i);
            }
            focused_card = -1;
            switch_main_phase(PLAYER, MAIN_BATTLE);
            goto post_logic;
        }

        int new_focused_card;
        if (mouse_x >= STATUS_X && mouse_y >= STATUS_HAND_Y + 20 &&
            mouse_y < STATUS_HAND_Y + STATUS_HAND_H)
        {
            new_focused_card = (mouse_y - STATUS_HAND_Y - 20) / 40;
            if (new_focused_card >= party_hand_size(PLAYER)) {
                new_focused_card = -1;
            }
        } else {
            new_focused_card = -1;
        }
        if (new_focused_card != focused_card) {
            focused_card = new_focused_card;
            refresh_hand();
        }

        if (focused_card >= 0 && lbuttondown) {
            if (party_hand_card(PLAYER, focused_card)->selected) {
                queue_sfx(&beep2_snd);
                party_hand_deselect_card(PLAYER, focused_card);
            } else {
                queue_sfx(&beep1_snd);
                party_hand_select_card(PLAYER, focused_card);
            }
            refresh_hand();
        }

        if (key_pressed == KEY_ENTER) {
            if (!trade_in_selected_cards(PLAYER)) {
                invalid_move("This is not a valid set of cards to trade in");
            } else if (!has_any_card_set(PLAYER)) {
                focused_card = -1;
                if (main_phase[PLAYER] == MAIN_TRADE_IN_CARDS) {
                    switch_main_phase(PLAYER, MAIN_REINFORCEMENT);
                } else {
                    switch_main_phase(PLAYER, MAIN_BATTLE);
                }
            } else {
                refresh_hand();

                if (party_hand_size(PLAYER) < 5) {
                    clear_to_bg(STATUS_X, STATUS_INFO_Y, fbw - STATUS_X,
                                STATUS_INFO_H);

                    font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X,
                                   STATUS_INFO_Y,
                                   "Press enter to confirm, or the space bar "
                                   "to skip.", 0);

                    platform_funcs.fb_flush(STATUS_X, STATUS_INFO_Y,
                                            fbw - STATUS_X, STATUS_INFO_H);
                }
            }

            goto post_logic;
        }
    } else if (main_phase[active_party] == MAIN_TRADE_IN_CARDS ||
               main_phase[active_party] == MAIN_BATTLE_TRADE_IN_CARDS)
    {
        uint64_t now = platform_funcs.elapsed_us();
        if (ai_do_trade_in_timestamp <= now) {
            ai_do_trade_in_timestamp = (uint64_t)-1;
            bool ret = trade_in_selected_cards(active_party);
            assert(ret);
            refresh_hand();
            ai_select_trade_in_timestamp = now + 1000000;
        } else if (ai_select_trade_in_timestamp <= now) {
            if (ai_select_trade_in_cards(active_party)) {
                refresh_hand();
                ai_do_trade_in_timestamp = now + 3000000;
                ai_select_trade_in_timestamp = (uint64_t)-1;
            } else if (main_phase[active_party] == MAIN_TRADE_IN_CARDS) {
                switch_main_phase(active_party, MAIN_REINFORCEMENT);
            } else {
                switch_main_phase(active_party, MAIN_BATTLE);
            }
        }
        goto post_logic;
    }

    if (game_phase == PREPARATION) {
        bool all_troops_placed = true;
        for (Party p = 0; p < PARTY_COUNT; p++) {
            if (troops_to_place[p]) {
                all_troops_placed = false;
                break;
            }
        }

        if (all_troops_placed) {
            REFRESH_INCLUDE_REGION_TROOPS(ai_focused_region);
            ai_focused_region = NULL_REGION;
            switch_game_phase(MAIN);
            goto post_logic;
        }
    }


    static bool ai_needs_to_place_in_prep;
    if (game_phase == PREPARATION && ai_needs_to_place_in_prep) {
        bool ai_needs_to_place = false;
        for (Party p = 0; p < PARTY_COUNT; p++) {
            if (ai_place_troops_stamp1[p] ||
                ai_place_troops_stamp2[p] ||
                ai_place_neutral_troops_stamp[p])
            {
                ai_needs_to_place = true;
                break;
            }
        }

        if (ai_needs_to_place_in_prep && !ai_needs_to_place) {
            ai_needs_to_place_in_prep = false;

            clear_to_bg(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                        STATUS_INFO_Y + STATUS_INFO_H - STATUS_TODO_Y);

            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
#ifdef HAVE_NEUTRAL
                           "Reinforce your regions by placing two additional "
                           "armies per turn.",
#else
                           "Reinforce your regions by placing additional "
                           "troops.",
#endif
                           0);

            char buf[64];
            snprintf(buf, sizeof(buf),
                     "You have %i %s remaining.",
                     troops_to_place[PLAYER],
                     troops_to_place[PLAYER] == 1 ? "army" : "armies");

            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_INFO_Y,
                           buf, 0);

            platform_funcs.fb_flush(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                                    STATUS_INFO_Y + STATUS_INFO_H -
                                    STATUS_TODO_Y);
        }

        if (ai_needs_to_place_in_prep) {
            uint64_t now = platform_funcs.elapsed_us();
            for (Party p = 0; p < PARTY_COUNT; p++) {
                if (ai_place_troops_stamp1[p] &&
                    ai_place_troops_stamp1[p] <= now)
                {
                    ai_place_troops(p);
                    ai_place_troops_stamp1[p] = 0;
                    REFRESH_INCLUDE(0, 0, fbw, fbh);
                }
#ifdef HAVE_NEUTRAL
                if (ai_place_troops_stamp2[p] &&
                    ai_place_troops_stamp2[p] <= now)
                {
                    ai_place_troops(p);
                    ai_place_troops_stamp2[p] = 0;
                    REFRESH_INCLUDE(0, 0, fbw, fbh);
                }
                if (ai_place_neutral_troops_stamp[p] &&
                    ai_place_neutral_troops_stamp[p] <= now)
                {
                    ai_place_neutral_troops(p);
                    ai_place_neutral_troops_stamp[p] = 0;
                    REFRESH_INCLUDE(0, 0, fbw, fbh);
                }
#endif
            }
        }
    } else {
        ai_needs_to_place_in_prep = false;
    }

    if ((game_phase == PREPARATION ||
        (game_phase == MAIN && main_phase[PLAYER] == MAIN_REINFORCEMENT)) &&
        focused_region && lbuttondown && !ai_needs_to_place_in_prep)
    {
        Party p;

        if (game_phase != PREPARATION) {
            preparation_placement_index = 0;
        }

#ifdef HAVE_NEUTRAL
        if (preparation_placement_index == 2) {
            p = NEUTRAL;
        } else {
            p = PLAYER;
        }
#else
        p = PLAYER;
#endif
        assert(troops_to_place[p] > 0);

        int unclaimed = 0;
        for (RegionID r = 1; r < REGION_COUNT; r++) {
            if (!regions[r].troops) {
                unclaimed++;
            }
        }

#ifdef HAVE_NEUTRAL
        assert(!unclaimed && regions[focused_region].troops);
#endif

        if (game_phase == MAIN) {
            assert(!unclaimed && regions[focused_region].troops);
        }

        if (regions[focused_region].troops) {
            if (regions[focused_region].taken_by != p) {
                if (p == PLAYER) {
                    invalid_move("Cannot place troops on enemy-controlled "
                                 "region.");
                } else {
                    invalid_move("Cannot place troops on non-neutral region.");
                }
                goto post_logic;
            }

            if (unclaimed) {
                invalid_move("All regions must be taken before you can "
                             "place reinforcements.");
                goto post_logic;
            }

            regions[focused_region].troops++;

            queue_sfx(&reinforcements_snd);
        } else {
            regions[focused_region].taken_by = p;
            regions[focused_region].troops = 1;

            queue_sfx(&reinforcements_snd);
        }

        // I'm so lazy
        REFRESH_INCLUDE(0, 0, fbw, fbh);

        troops_to_place[p]--;

#ifdef HAVE_NEUTRAL
        if (game_phase == PREPARATION) {
            preparation_placement_index = (preparation_placement_index + 1) % 3;
        }
#endif

        if (unclaimed <= PARTY_COUNT) {
            clear_to_bg(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                        STATUS_INFO_Y + STATUS_INFO_H - STATUS_TODO_Y);

            bool ai_is_next =
                game_phase == PREPARATION && preparation_placement_index == 0;

            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           ai_is_next ?
                                "Wait for your opponent to place troops..." :
                           preparation_placement_index == 2 ?
                               "Reinforce the neutral troops with one army." :
#ifdef HAVE_NEUTRAL
                               "Reinforce your regions by placing two additional "
                               "armies per turn.",
#else
                               "Reinforce your regions by placing additional "
                               "troops.",
#endif
                           0);

#ifdef HAVE_NEUTRAL
            p = preparation_placement_index == 2 ? NEUTRAL : PLAYER;
#endif

            char buf[64];
            if (ai_is_next) {
                buf[0] = 0;
            } else if (p == PLAYER) {
                snprintf(buf, sizeof(buf),
                         "You have %i %s remaining.",
                         troops_to_place[p],
                         troops_to_place[p] == 1 ? "army" : "armies");
            } else {
                snprintf(buf, sizeof(buf),
                         "There %s %i neutral %s remaining.",
                         troops_to_place[p] == 1 ? "is" : "are",
                         troops_to_place[p],
                         troops_to_place[p] == 1 ? "army" : "armies");
            }

            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_INFO_Y,
                           buf, 0);
            platform_funcs.fb_flush(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                                    STATUS_INFO_Y + STATUS_INFO_H -
                                    STATUS_TODO_Y);
        }


        if (game_phase == PREPARATION && preparation_placement_index == 0) {
            uint64_t stamp = platform_funcs.elapsed_us();
            for (Party aip = 0; aip < PARTY_COUNT; aip++) {
                if (aip == PLAYER || aip == NEUTRAL) {
                    continue;
                }
                stamp += 1000000;
                ai_place_troops_stamp1[aip] = stamp;
#ifdef HAVE_NEUTRAL
                stamp += 1000000;
                ai_place_troops_stamp2[aip] = stamp;
                stamp += 1000000;
                ai_place_neutral_troops_stamp[aip] = stamp;
#endif
                ai_needs_to_place_in_prep = true;
            }
        }

        if (game_phase == MAIN && !troops_to_place[PLAYER]) {
            switch_main_phase(PLAYER, MAIN_BATTLE);
        }

        goto post_logic;
    }

    if (game_phase == MAIN && main_phase[PLAYER] == MAIN_BATTLE &&
        focused_region && lbuttondown)
    {
        integer_prompt = NO_PROMPT;

        if (!attacking_region) {
            if (regions[focused_region].taken_by != PLAYER) {
                invalid_move("This region does not belong to you.");
                goto post_logic;
            } else if (regions[focused_region].troops < 2) {
                invalid_move("To attack, you need at least two armies.");
                goto post_logic;
            }

            attacking_region = focused_region;
            REFRESH_INCLUDE_REGION_TROOPS(attacking_region);
            queue_sfx(&beep1_snd);
        } else if (attacking_region == focused_region) {
            REFRESH_INCLUDE_REGION_TROOPS(attacking_region);
            attacking_region = NULL_REGION;
            if (defending_region) {
                REFRESH_INCLUDE_REGION_TROOPS(defending_region);
                defending_region = NULL_REGION;
            }
            queue_sfx(&beep2_snd);
        } else if (!defending_region) {
            if (regions[focused_region].taken_by == PLAYER) {
                invalid_move("You can only attack enemy-controlled regions.");
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
                             "with the attacking region.");
                goto post_logic;
            }

            defending_region = focused_region;
            REFRESH_INCLUDE_REGION_TROOPS(defending_region);
            queue_sfx(&beep1_snd);
        } else if (defending_region == focused_region) {
            REFRESH_INCLUDE_REGION_TROOPS(defending_region);
            defending_region = NULL_REGION;
            queue_sfx(&beep2_snd);
        } else {
            invalid_move("Battles only involve two regions.");
            goto post_logic;
        }

        clear_to_bg(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X, STATUS_TODO_H);
        if (!attacking_region) {
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose a region to attack from.", 0);
        } else if (!defending_region) {
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose an enemy-controlled region to attack.", 0);
        } else {
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose how many armies you want to attack with "
                           "(1, 2, or 3).", 0);
            integer_prompt = ATTACK_TROOPS_COUNT;
        }
        platform_funcs.fb_flush(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                                STATUS_TODO_H);

        goto post_logic;
    }

    if (game_phase == MAIN && main_phase[PLAYER] == MAIN_BATTLE &&
        integer_prompt_done == ATTACK_TROOPS_COUNT)
    {
        assert(attacking_region && defending_region);

        attacking_count = integer_prompt_value;
        integer_prompt_done = NO_PROMPT;

        if (attacking_count < 1 || attacking_count > 3) {
            invalid_move("Invalid number of attacking armies.");
            attacking_count = 0;
        } else if (regions[attacking_region].troops < attacking_count) {
            invalid_move("Insufficient armies in the attacking region.");
            attacking_count = 0;
        } else if (regions[attacking_region].troops < attacking_count + 1) {
            invalid_move("Cannot attack with this many armies (at least one "
                         "must be left behind).");
            attacking_count = 0;
        }

        defending_count = 0;
        if (attacking_count) {
            defending_count = ai_choose_defense_count();
        }

        REFRESH_INCLUDE_REGION_TROOPS(attacking_region);
        REFRESH_INCLUDE_REGION_TROOPS(defending_region);

        if (attacking_count && battle()) {
            if (game_phase == GAME_OVER) {
                goto post_logic;
            }

            if (regions[attacking_region].troops == 1) {
                // No human intervention required
                integer_prompt_done = CLAIM_TROOPS_COUNT;
                integer_prompt_value = 0;
            } else {
                clear_to_bg(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                            STATUS_TODO_H);
                font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                               "Choose how many armies you want to move to the "
                               "conquered region.", 0);
                platform_funcs.fb_flush(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                                        STATUS_TODO_H);

                integer_prompt = CLAIM_TROOPS_COUNT;
            }
        } else {
            attacking_region = defending_region = NULL_REGION;

            // I cannot be lazy and use switch_main_phase(PLAYER, MAIN_BATTLE)
            // here because that would clear the whole side bar, but the dice
            // should stay visible
            clear_to_bg(STATUS_X, 0, fbw - STATUS_X,
                        STATUS_INFO_Y + STATUS_INFO_H);
            ablitlmt(fb, main_phase_headings[MAIN_BATTLE], STATUS_X,
                     STATUS_PHASE_Y, headings_w, headings_h, fb_stride,
                     headings_w * sizeof(uint32_t), STATUS_X, STATUS_PHASE_Y,
                     fbw, STATUS_PHASE_Y + STATUS_PHASE_H);
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose a region to attack from.", 0);
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_INFO_Y,
                           "Press the space bar to end the battle phase.",
                           0);
            platform_funcs.fb_flush(STATUS_X, 0, fbw - STATUS_X,
                                    STATUS_INFO_Y + STATUS_INFO_H);
        }

        goto post_logic;
    }

    if (game_phase == MAIN && main_phase[PLAYER] == MAIN_BATTLE &&
        integer_prompt_done == CLAIM_TROOPS_COUNT)
    {
        assert(attacking_region && defending_region);

        int moving_troops = integer_prompt_value;
        integer_prompt_done = NO_PROMPT;

        if (regions[attacking_region].troops < moving_troops) {
            invalid_move("Insufficient armies in the attacking region.");
            integer_prompt = CLAIM_TROOPS_COUNT;
            goto post_logic;
        } else if (regions[attacking_region].troops == moving_troops) {
            invalid_move("At least one army has to stay behind.");
            integer_prompt = CLAIM_TROOPS_COUNT;
            goto post_logic;
        }

        regions[attacking_region].troops -= moving_troops;
        regions[defending_region].troops += moving_troops;

        if (moving_troops) {
            // Less strong than movement_snd (which happens just once
            // per turn)
            queue_sfx(&reinforcements_snd);
        }

        REFRESH_INCLUDE_REGION_TROOPS(attacking_region);
        REFRESH_INCLUDE_REGION_TROOPS(defending_region);
        attacking_region = defending_region = NULL_REGION;

        // I'm lazy
        switch_main_phase(PLAYER, MAIN_BATTLE);

        goto post_logic;
    }

    if (game_phase == MAIN && main_phase[PLAYER] == MAIN_BATTLE &&
        key_pressed == KEY_SPACE)
    {
        integer_prompt = NO_PROMPT;
        if (attacking_region) {
            REFRESH_INCLUDE_REGION_TROOPS(attacking_region);
            attacking_region = NULL_REGION;
        }
        if (defending_region) {
            REFRESH_INCLUDE_REGION_TROOPS(defending_region);
            defending_region = NULL_REGION;
        }
        switch_main_phase(PLAYER, MAIN_MOVEMENT);
        goto post_logic;
    }

    if (game_phase == MAIN && ai_waiting_for_defending_count) {
        Party p = regions[defending_region].taken_by;

#ifdef HAVE_NEUTRAL
        assert(p == PLAYER || p == NEUTRAL);
#else
        assert(p == PLAYER);
#endif

        if (regions[defending_region].troops == 1) {
            // No need for human intervention
            defending_count = 1;
            ai_waiting_for_defending_count = false;
        } else if (!integer_prompt && !integer_prompt_done) {
            clear_to_bg(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X, STATUS_TODO_H);
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           p == PLAYER ? "Choose how many armies you want to "
                                         "defend with (1 or 2)."
                                       : "Choose how many neutral armies "
                                         "should be used for defense (1 or 2).",
                           0);
            platform_funcs.fb_flush(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                                    STATUS_TODO_Y);

            // Notify the player because they may have stopped paying
            // attention during their opponent's turn
            important_message(p == PLAYER ? "Choose how many armies you want "
                                            "to defend with."
                                          : "Choose how many neutral armies "
                                            "should be used for defense.",
                              false);

            integer_prompt = DEFENSE_TROOPS_COUNT;
        } else if (integer_prompt_done) {
            assert(integer_prompt_done == DEFENSE_TROOPS_COUNT);
            integer_prompt_done = NO_PROMPT;

            defending_count = integer_prompt_value;

            if (defending_count < 1 || defending_count > 2) {
                invalid_move("Invalid number of defending armies.");
                integer_prompt = DEFENSE_TROOPS_COUNT;
                defending_count = 0;
            } else if (regions[defending_region].troops < defending_count) {
                invalid_move("Not enough armies in the defending region.");
                integer_prompt = DEFENSE_TROOPS_COUNT;
                defending_count = 0;
            }

            if (defending_count) {
                clear_to_bg(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                            STATUS_TODO_H);
                platform_funcs.fb_flush(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                                        STATUS_TODO_Y);

                ai_waiting_for_defending_count = false;
            }
        }
    }

    if (game_phase == MAIN && main_phase[PLAYER] == MAIN_MOVEMENT &&
        focused_region && lbuttondown)
    {
        integer_prompt = NO_PROMPT;

        if (!origin_region) {
            if (regions[focused_region].taken_by != PLAYER) {
                invalid_move("This region does not belong to you.");
                goto post_logic;
            } else if (regions[focused_region].troops < 2) {
                invalid_move("To move troops, you need at least two armies "
                             "in the origin region.");
                goto post_logic;
            }

            origin_region = focused_region;
            REFRESH_INCLUDE_REGION_TROOPS(origin_region);
            queue_sfx(&beep1_snd);
        } else if (origin_region == focused_region) {
            REFRESH_INCLUDE_REGION_TROOPS(origin_region);
            origin_region = NULL_REGION;
            if (destination_region) {
                REFRESH_INCLUDE_REGION_TROOPS(destination_region);
                destination_region = NULL_REGION;
            }
            queue_sfx(&beep2_snd);
        } else if (!destination_region) {
            if (regions[focused_region].taken_by != PLAYER) {
                invalid_move("This region does not belong to you.");
                goto post_logic;
            }
            // No empty regions
            assert(regions[focused_region].troops);

            bool is_connected = friendly_connection(origin_region,
                                                    focused_region);
            if (!is_connected) {
                invalid_move("You cannot move troops through enemy regions.");
                goto post_logic;
            }

            destination_region = focused_region;
            REFRESH_INCLUDE_REGION_TROOPS(destination_region);
            queue_sfx(&beep1_snd);
        } else if (destination_region == focused_region) {
            REFRESH_INCLUDE_REGION_TROOPS(destination_region);
            destination_region = NULL_REGION;
            queue_sfx(&beep2_snd);
        } else {
            invalid_move("Movement is allowed only between two regions.");
            goto post_logic;
        }

        clear_to_bg(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X, STATUS_TODO_H);
        if (!origin_region) {
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose one region to move troops from.", 0);
        } else if (!destination_region) {
            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose one region to move troops to.", 0);
        } else {
            // Should have been caught by the conditions above
            assert(destination_region != origin_region);

            font_draw_text(fb, fbw, fbh, fb_stride, STATUS_X, STATUS_TODO_Y,
                           "Choose how many armies you want to move.", 0);
            integer_prompt = MOVE_TROOPS_COUNT;
        }
        platform_funcs.fb_flush(STATUS_X, STATUS_TODO_Y, fbw - STATUS_X,
                                STATUS_TODO_H);

        goto post_logic;
    }

    if (game_phase == MAIN && main_phase[PLAYER] == MAIN_MOVEMENT &&
        integer_prompt_done == MOVE_TROOPS_COUNT)
    {
        assert(origin_region && destination_region);

        int moving_troops = integer_prompt_value;
        integer_prompt_done = NO_PROMPT;

        if (moving_troops <= 0) {
            invalid_move("Invalid number of armies to move.");
            integer_prompt = MOVE_TROOPS_COUNT;
            goto post_logic;
        } else if (regions[origin_region].troops < moving_troops) {
            invalid_move("Insufficient armies in the origin region.");
            integer_prompt = MOVE_TROOPS_COUNT;
            goto post_logic;
        } else if (regions[origin_region].troops == moving_troops) {
            invalid_move("At least one army has to stay behind.");
            integer_prompt = MOVE_TROOPS_COUNT;
            goto post_logic;
        }

        regions[origin_region].troops -= moving_troops;
        regions[destination_region].troops += moving_troops;

        queue_sfx(&movement_snd);

        REFRESH_INCLUDE_REGION_TROOPS(origin_region);
        REFRESH_INCLUDE_REGION_TROOPS(destination_region);
        origin_region = destination_region = NULL_REGION;

        switch_main_phase(PLAYER, MAIN_WAITING_FOR_OTHER);

        goto post_logic;
    }

    if (game_phase == MAIN && main_phase[PLAYER] == MAIN_MOVEMENT &&
        key_pressed == KEY_SPACE)
    {
        integer_prompt = NO_PROMPT;
        if (origin_region) {
            REFRESH_INCLUDE_REGION_TROOPS(origin_region);
            origin_region = NULL_REGION;
        }
        if (destination_region) {
            REFRESH_INCLUDE_REGION_TROOPS(destination_region);
            destination_region = NULL_REGION;
        }
        switch_main_phase(PLAYER, MAIN_WAITING_FOR_OTHER);
        goto post_logic;
    }

    if (game_phase == MAIN) {
        static uint64_t next_ai_move;
        uint64_t now = platform_funcs.elapsed_us();

        if (now >= next_ai_move) {
            for (Party p = 0; p < PARTY_COUNT; p++) {
                if (p != PLAYER && main_phase[p] != MAIN_WAITING_FOR_OTHER) {
                    ai_move(p);
                    REFRESH_INCLUDE(0, 0, fbw, fbh);
                    break;
                }
            }

            next_ai_move = now + 1000000;
        }
    }

post_logic:
    if (refresh_xmax > refresh_xmin) {
        refresh(refresh_xmin, refresh_ymin, refresh_xmax, refresh_ymax);
    }
}
