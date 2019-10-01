// Copyright (c) 2019 Xnor.ai, Inc.
//
#ifndef __COMMON_UTIL_COLORS_H__
#define __COMMON_UTIL_COLORS_H__

#include <stdint.h>

typedef struct xg_color {
  uint8_t r, g, b, a;
} xg_color;

const extern xg_color xg_color_palette[];
const extern int32_t xg_color_palette_length;

#endif  // __COMMON_UTIL_COLORS_H__
