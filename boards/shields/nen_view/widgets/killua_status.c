#include <zephyr/kernel.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

#include "killua_status.h"
#include "killua_bitmap.h"

/* --- Activity Tracking ---
 * Counts key presses within a sliding time window on the peripheral side.
 * This drives the aura intensity since WPM isn't available on peripheral.
 */

#define ACTIVITY_WINDOW_MS 3000
#define ACTIVITY_BUF_SIZE  32

static int64_t press_times[ACTIVITY_BUF_SIZE];
static uint8_t press_idx;
static uint8_t press_count;

/* --- Nen Aura Levels --- */

enum aura_level {
    AURA_TEN = 0,      /* Idle - basic Nen defense, no visible aura */
    AURA_REN = 1,       /* Low activity - small sparks from hair */
    AURA_HATSU = 2,     /* Medium - electric field around body */
    AURA_GODSPEED = 3,  /* High - full Godspeed activation */
};

static const char *aura_names[] = {"Ten", "Ren", "Hatsu", "Godspeed"};
static const int bolts_per_level[] = {0, 3, 8, 12};

/* --- Display Objects --- */

/* Sprite centered on 160x68 nice!view display */
#define SPRITE_X  68
#define SPRITE_Y  14
#define NUM_BOLTS 12

static lv_obj_t *aura_label;
static lv_obj_t *sprite_canvas;
static lv_obj_t *bolt_objs[NUM_BOLTS];
static int current_level = -1;

/* Canvas buffer for the sprite (small - only sprite area) */
static lv_color_t sprite_buf[KILLUA_WIDTH * KILLUA_HEIGHT];

/* --- Lightning Bolt Definitions ---
 * Zigzag polylines radiating from Killua's body.
 * Sprite is at (68,14), size 24x40, center ~(80,34).
 *
 * Level 1 (Ren):      bolts 0-2   (hair sparks)
 * Level 2 (Hatsu):    bolts 0-7   (upper body + sides)
 * Level 3 (Godspeed): bolts 0-11  (full body electric field)
 */

/* Top bolts - from hair area */
static lv_point_t bolt_pts_0[]  = {{76, 14}, {73, 7},  {78, 2},  {72, 0}};
static lv_point_t bolt_pts_1[]  = {{80, 14}, {83, 6},  {78, 1},  {82, 0}};
static lv_point_t bolt_pts_2[]  = {{84, 14}, {88, 7},  {85, 3},  {90, 0}};

/* Right side bolts */
static lv_point_t bolt_pts_3[]  = {{92, 26}, {100, 24}, {97, 29}, {105, 27}};
static lv_point_t bolt_pts_4[]  = {{92, 36}, {100, 34}, {98, 40}, {108, 38}};
static lv_point_t bolt_pts_5[]  = {{90, 50}, {98, 52},  {96, 57}, {102, 60}};

/* Bottom bolts */
static lv_point_t bolt_pts_6[]  = {{84, 54}, {86, 58}, {82, 62}, {85, 66}};
static lv_point_t bolt_pts_7[]  = {{76, 54}, {74, 58}, {78, 63}, {75, 66}};

/* Left side bolts */
static lv_point_t bolt_pts_8[]  = {{68, 50}, {60, 52}, {62, 57}, {55, 60}};
static lv_point_t bolt_pts_9[]  = {{68, 36}, {58, 34}, {62, 40}, {52, 38}};
static lv_point_t bolt_pts_10[] = {{68, 26}, {58, 24}, {62, 20}, {52, 18}};
static lv_point_t bolt_pts_11[] = {{72, 14}, {66, 8},  {70, 3},  {62, 0}};

static lv_point_t *bolt_pts_all[NUM_BOLTS] = {
    bolt_pts_0,  bolt_pts_1,  bolt_pts_2,  bolt_pts_3,
    bolt_pts_4,  bolt_pts_5,  bolt_pts_6,  bolt_pts_7,
    bolt_pts_8,  bolt_pts_9,  bolt_pts_10, bolt_pts_11,
};

#define BOLT_POINT_COUNT 4

/* --- Bolt line style --- */
static lv_style_t bolt_style;

/* --- Draw the Killua sprite onto the canvas --- */

static void draw_sprite(void) {
    /* Fill canvas with white (background) */
    lv_canvas_fill_bg(sprite_canvas, lv_color_white(), LV_OPA_COVER);

    /* Draw black pixels from bitmap data */
    for (int y = 0; y < KILLUA_HEIGHT; y++) {
        for (int x = 0; x < KILLUA_WIDTH; x++) {
            if (killua_pixel(x, y)) {
                lv_canvas_set_px_color(sprite_canvas, x, y, lv_color_black());
            }
        }
    }
}

/* --- Activity measurement --- */

static int get_activity_count(void) {
    int64_t now = k_uptime_get();
    int count = 0;
    int n = MIN(press_count, ACTIVITY_BUF_SIZE);

    for (int i = 0; i < n; i++) {
        if ((now - press_times[i]) <= ACTIVITY_WINDOW_MS) {
            count++;
        }
    }
    return count;
}

static enum aura_level calc_aura_level(void) {
    int count = get_activity_count();

    if (count == 0) {
        return AURA_TEN;
    }
    if (count < 8) {
        return AURA_REN;
    }
    if (count < 20) {
        return AURA_HATSU;
    }
    return AURA_GODSPEED;
}

/* --- Display update (runs on system workqueue) --- */

static void update_display(struct k_work *work) {
    enum aura_level level = calc_aura_level();

    if (level == current_level) {
        return;
    }
    current_level = level;

    lv_label_set_text(aura_label, aura_names[level]);

    int num_visible = bolts_per_level[level];
    for (int i = 0; i < NUM_BOLTS; i++) {
        if (i < num_visible) {
            lv_obj_clear_flag(bolt_objs[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(bolt_objs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static K_WORK_DEFINE(display_work, update_display);

static void display_timer_cb(struct k_timer *timer) {
    k_work_submit(&display_work);
}

/* Periodic timer to decay aura back to Ten when idle */
static K_TIMER_DEFINE(display_timer, display_timer_cb, NULL);

/* --- Key event listener ---
 * Tracks local key presses on the peripheral half.
 * Each press is timestamped for the sliding activity window.
 */

static int nen_activity_cb(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    if (ev && ev->state) {
        press_times[press_idx] = k_uptime_get();
        press_idx = (press_idx + 1) % ACTIVITY_BUF_SIZE;
        if (press_count < ACTIVITY_BUF_SIZE) {
            press_count++;
        }
        k_work_submit(&display_work);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(nen_activity, nen_activity_cb);
ZMK_SUBSCRIPTION(nen_activity, zmk_position_state_changed);

/* --- Status screen entry point ---
 * Called by ZMK display subsystem when CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM=y.
 * Creates the full screen layout: aura label + Killua canvas + lightning bolts.
 */

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    /* Aura level label - top left corner */
    aura_label = lv_label_create(screen);
    lv_label_set_text(aura_label, "Ten");
    lv_obj_align(aura_label, LV_ALIGN_TOP_LEFT, 4, 2);

    /* Killua sprite - drawn on a small canvas */
    sprite_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(sprite_canvas, sprite_buf,
                         KILLUA_WIDTH, KILLUA_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(sprite_canvas, SPRITE_X, SPRITE_Y);
    draw_sprite();

    /* Bolt line style - thin black zigzag lines */
    lv_style_init(&bolt_style);
    lv_style_set_line_color(&bolt_style, lv_color_black());
    lv_style_set_line_width(&bolt_style, 1);
    lv_style_set_line_rounded(&bolt_style, false);

    /* Create all lightning bolt lines (hidden until aura activates) */
    for (int i = 0; i < NUM_BOLTS; i++) {
        bolt_objs[i] = lv_line_create(screen);
        lv_line_set_points(bolt_objs[i], bolt_pts_all[i], BOLT_POINT_COUNT);
        lv_obj_add_style(bolt_objs[i], &bolt_style, 0);
        lv_obj_add_flag(bolt_objs[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Start periodic refresh to decay aura back to Ten when idle */
    k_timer_start(&display_timer, K_MSEC(500), K_MSEC(500));

    return screen;
}
