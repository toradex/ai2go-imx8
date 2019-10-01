// Copyright (c) 2019 Xnor.ai, Inc.
//
#ifndef __COMMON_UTIL_OVERLAYS_H__
#define __COMMON_UTIL_OVERLAYS_H__

#include <stdint.h>
#include <stdbool.h>

#include <cairo.h>

#include "colors.h"

enum xg_overlay_type {
  XG_OVERLAY_TEXT,
  XG_OVERLAY_BOUNDING_BOX,
};

typedef struct xg_overlay {
  enum xg_overlay_type type;
  float x, y;
  char* text;
  xg_color bg_color, text_color;
  // bounding boxes only
  float width, height;
  // linked list functionality
  struct xg_overlay* next;
  bool owned_by_pipeline;
} xg_overlay;

enum {
  // Height of the overlay text, in pixels. Used as the font size when drawing
  // text in cairo.
  OVERLAY_TEXT_SIZE = 24
};

xg_overlay* xg_overlay_create_text(float x, float y, const char* label,
                                   xg_color color);
xg_overlay* xg_overlay_create_bounding_box(float x, float y, float width,
                                           float height, const char* label,
                                           xg_color color);
void xg_overlay_draw(xg_overlay* overlay, cairo_t* cr, int32_t surface_width,
                     int32_t surface_height);
void xg_overlay_free(xg_overlay* overlay);

#endif  // __COMMON_UTIL_OVERLAYS_H__
