/*
 * Minimal central status widget.
 * This file is compiled for the central side but won't be used in practice
 * since the left half uses the stock nice_view shield.
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include "status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    lv_obj_t *label = lv_label_create(widget->obj);
    lv_label_set_text(label, "NEN");
    lv_obj_center(label);

    sys_slist_append(&widgets, &widget->node);
    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
