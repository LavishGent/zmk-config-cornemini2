/*
 * Killua Zoldyck peripheral display widget with Nen aura system.
 * Renders a chibi Killua sprite with dynamic lightning bolts that
 * intensify based on typing activity.
 *
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/usb.h>

#include "status.h"
#include "killua_bitmap.h"

/* ------------------------------------------------------------------ */
/* Killua full-screen image (160x68, INDEXED_1BIT)                    */
/* Generated at init from the 24x40 sprite data in killua_bitmap.h    */
/* ------------------------------------------------------------------ */

#define IMG_WIDTH   160
#define IMG_HEIGHT  68
#define IMG_STRIDE  20   /* 160 / 8 */
#define IMG_PIXELS  (IMG_STRIDE * IMG_HEIGHT)
#define IMG_TOTAL   (8 + IMG_PIXELS) /* 8-byte palette + pixel data */

#define SPRITE_X 68
#define SPRITE_Y 14

static uint8_t killua_img_buf[IMG_TOTAL];

static lv_img_dsc_t killua_img_dsc = {
    .header.cf = LV_IMG_CF_INDEXED_1BIT,
    .header.w = IMG_WIDTH,
    .header.h = IMG_HEIGHT,
    .data_size = IMG_TOTAL,
    .data = killua_img_buf,
};

static void generate_killua_image(void) {
    uint8_t *buf = killua_img_buf;

    /* Palette: index 0 = foreground, index 1 = background */
#if IS_ENABLED(CONFIG_NEN_VIEW_WIDGET_INVERTED)
    buf[0] = 0xFF; buf[1] = 0xFF; buf[2] = 0xFF; buf[3] = 0xFF;
    buf[4] = 0x00; buf[5] = 0x00; buf[6] = 0x00; buf[7] = 0xFF;
#else
    buf[0] = 0x00; buf[1] = 0x00; buf[2] = 0x00; buf[3] = 0xFF;
    buf[4] = 0xFF; buf[5] = 0xFF; buf[6] = 0xFF; buf[7] = 0xFF;
#endif

    uint8_t *pixels = buf + 8;

    /* Fill with background (all 1s = index 1 = white) */
    memset(pixels, 0xFF, IMG_PIXELS);

    /* Draw Killua sprite: clear bits where sprite has filled pixels.
     * Sprite is 24px wide at screen x=68.
     * Screen byte 8 starts at column 64, so sprite column 0 (screen col 68)
     * lands at byte 8, bit 3. The 24 sprite pixels span bytes 8-11.
     */
    for (int sy = 0; sy < KILLUA_HEIGHT; sy++) {
        int row = SPRITE_Y + sy;
        uint8_t *rp = pixels + row * IMG_STRIDE;

        uint8_t sb0 = killua_map[sy * KILLUA_STRIDE + 0];
        uint8_t sb1 = killua_map[sy * KILLUA_STRIDE + 1];
        uint8_t sb2 = killua_map[sy * KILLUA_STRIDE + 2];

        /* Sprite bits that are 1 = black → clear corresponding image bits to 0 */
        rp[8]  &= ~(sb0 >> 4);
        rp[9]  &= ~(((sb0 & 0x0F) << 4) | (sb1 >> 4));
        rp[10] &= ~(((sb1 & 0x0F) << 4) | (sb2 >> 4));
        rp[11] &= ~((sb2 & 0x0F) << 4);
    }
}

/* ------------------------------------------------------------------ */
/* Lightning bolt definitions                                         */
/* ------------------------------------------------------------------ */

#define NUM_BOLTS    12
#define BOLT_POINTS  4

/* Aura levels */
enum aura_level { AURA_TEN = 0, AURA_REN = 1, AURA_HATSU = 2, AURA_GODSPEED = 3 };
static const char *aura_names[] = {"Ten", "Ren", "Hatsu", "Godspeed"};
static const int bolts_per_level[] = {0, 3, 8, 12};

/* Top bolts (from hair) */
static lv_point_t bolt_pts_0[]  = {{76, 14}, {73, 7},  {78, 2},  {72, 0}};
static lv_point_t bolt_pts_1[]  = {{80, 14}, {83, 6},  {78, 1},  {82, 0}};
static lv_point_t bolt_pts_2[]  = {{84, 14}, {88, 7},  {85, 3},  {90, 0}};
/* Right side */
static lv_point_t bolt_pts_3[]  = {{92, 26}, {100, 24}, {97, 29}, {105, 27}};
static lv_point_t bolt_pts_4[]  = {{92, 36}, {100, 34}, {98, 40}, {108, 38}};
static lv_point_t bolt_pts_5[]  = {{90, 50}, {98, 52},  {96, 57}, {102, 60}};
/* Bottom */
static lv_point_t bolt_pts_6[]  = {{84, 54}, {86, 58}, {82, 62}, {85, 66}};
static lv_point_t bolt_pts_7[]  = {{76, 54}, {74, 58}, {78, 63}, {75, 66}};
/* Left side */
static lv_point_t bolt_pts_8[]  = {{68, 50}, {60, 52}, {62, 57}, {55, 60}};
static lv_point_t bolt_pts_9[]  = {{68, 36}, {58, 34}, {62, 40}, {52, 38}};
static lv_point_t bolt_pts_10[] = {{68, 26}, {58, 24}, {62, 20}, {52, 18}};
static lv_point_t bolt_pts_11[] = {{72, 14}, {66, 8},  {70, 3},  {62, 0}};

static lv_point_t *bolt_pts_all[NUM_BOLTS] = {
    bolt_pts_0,  bolt_pts_1,  bolt_pts_2,  bolt_pts_3,
    bolt_pts_4,  bolt_pts_5,  bolt_pts_6,  bolt_pts_7,
    bolt_pts_8,  bolt_pts_9,  bolt_pts_10, bolt_pts_11,
};

static lv_obj_t *bolt_objs[NUM_BOLTS];
static lv_obj_t *aura_label;
static lv_style_t bolt_style;
static int current_aura = -1;

/* ------------------------------------------------------------------ */
/* Activity tracking                                                  */
/* ------------------------------------------------------------------ */

#define ACTIVITY_WINDOW_MS 3000
#define ACTIVITY_BUF_SIZE  32

static int64_t press_times[ACTIVITY_BUF_SIZE];
static uint8_t press_idx;
static uint8_t press_count;

static enum aura_level calc_aura_level(void) {
    int64_t now = k_uptime_get();
    int count = 0;
    int n = MIN(press_count, ACTIVITY_BUF_SIZE);

    for (int i = 0; i < n; i++) {
        if ((now - press_times[i]) <= ACTIVITY_WINDOW_MS) {
            count++;
        }
    }

    if (count == 0) return AURA_TEN;
    if (count < 8)  return AURA_REN;
    if (count < 20) return AURA_HATSU;
    return AURA_GODSPEED;
}

static void update_bolts(void) {
    enum aura_level level = calc_aura_level();
    if (level == current_aura) return;
    current_aura = level;

    lv_label_set_text(aura_label, aura_names[level]);

    int visible = bolts_per_level[level];
    for (int i = 0; i < NUM_BOLTS; i++) {
        if (i < visible) {
            lv_obj_clear_flag(bolt_objs[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(bolt_objs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void bolt_timer_cb(struct k_work *work) { update_bolts(); }
static K_WORK_DEFINE(bolt_work, bolt_timer_cb);

static void bolt_decay_cb(struct k_timer *timer) { k_work_submit(&bolt_work); }
static K_TIMER_DEFINE(bolt_timer, bolt_decay_cb, NULL);

/* Key press listener */
static int nen_activity_cb(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev && ev->state) {
        press_times[press_idx] = k_uptime_get();
        press_idx = (press_idx + 1) % ACTIVITY_BUF_SIZE;
        if (press_count < ACTIVITY_BUF_SIZE) press_count++;
        k_work_submit(&bolt_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(nen_activity, nen_activity_cb);
ZMK_SUBSCRIPTION(nen_activity, zmk_position_state_changed);

/* ------------------------------------------------------------------ */
/* Battery & connection (urchin-style top-right canvas)               */
/* ------------------------------------------------------------------ */

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct peripheral_status_state {
    bool connected;
};

static void draw_top(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);
    draw_battery(canvas, state);
    lv_canvas_draw_text(canvas, 0, 0, CANVAS_SIZE, &label_dsc,
                        state->connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);

    rotate_canvas(canvas, cbuf);
}

static void set_battery_status(struct zmk_widget_status *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    return (struct battery_status_state){
        .level = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

static struct peripheral_status_state get_state(const zmk_event_t *_eh) {
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected()};
}

static void set_connection_status(struct zmk_widget_status *widget,
                                  struct peripheral_status_state state) {
    widget->state.connected = state.connected;
    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void output_status_update_cb(struct peripheral_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_connection_status(widget, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);

/* ------------------------------------------------------------------ */
/* Widget init                                                        */
/* ------------------------------------------------------------------ */

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    /* Status canvas (battery + connection) in top-right, rotated 90deg */
    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);

    /* Killua sprite as full-screen INDEXED_1BIT image */
    generate_killua_image();
    lv_obj_t *art = lv_img_create(widget->obj);
    lv_img_set_src(art, &killua_img_dsc);
    lv_obj_align(art, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Aura level label */
    aura_label = lv_label_create(widget->obj);
    lv_label_set_text(aura_label, "Ten");
    lv_obj_set_style_text_font(aura_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(aura_label, LVGL_FOREGROUND, 0);
    lv_obj_align(aura_label, LV_ALIGN_BOTTOM_LEFT, 4, -4);

    /* Lightning bolt lines */
    lv_style_init(&bolt_style);
    lv_style_set_line_color(&bolt_style, LVGL_FOREGROUND);
    lv_style_set_line_width(&bolt_style, 1);
    lv_style_set_line_rounded(&bolt_style, false);

    for (int i = 0; i < NUM_BOLTS; i++) {
        bolt_objs[i] = lv_line_create(widget->obj);
        lv_line_set_points(bolt_objs[i], bolt_pts_all[i], BOLT_POINTS);
        lv_obj_add_style(bolt_objs[i], &bolt_style, 0);
        lv_obj_add_flag(bolt_objs[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Decay timer - checks every 500ms if aura should drop */
    k_timer_start(&bolt_timer, K_MSEC(500), K_MSEC(500));

    sys_slist_append(&widgets, &widget->node);
    widget_battery_status_init();
    widget_peripheral_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
