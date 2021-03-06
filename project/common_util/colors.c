// Copyright (c) 2019 Xnor.ai, Inc.
//
#include "colors.h"

/* 100 algorithmically generated colors for use with drawing model results
 *
 * In order to have 1) consistent and 2) distinguishable colors for bounding
 * boxes and labels in the demo, we can use some precomputed colors based on
 * random mixtures of a particular palette.
 */
const xg_color xg_color_palette[] = {
    {110, 227, 144, 255}, {226, 145, 112, 255}, {224, 147, 111, 255},
    {128, 206, 160, 255}, {229, 110, 196, 255}, {218, 119, 194, 255},
    {111, 193, 226, 255}, {214, 123, 196, 255}, {111, 194, 225, 255},
    {216, 151, 118, 255}, {183, 159, 167, 255}, {117, 219, 150, 255},
    {114, 198, 209, 255}, {215, 151, 118, 255}, {195, 145, 176, 255},
    {111, 226, 144, 255}, {117, 190, 221, 255}, {214, 125, 190, 255},
    {110, 226, 147, 255}, {115, 191, 224, 255}, {116, 190, 224, 255},
    {118, 188, 225, 255}, {115, 222, 146, 255}, {137, 191, 178, 255},
    {198, 164, 122, 255}, {145, 197, 148, 255}, {115, 223, 145, 255},
    {227, 143, 114, 255}, {124, 214, 148, 255}, {231, 137, 123, 255},
    {117, 221, 145, 255}, {223, 147, 112, 255}, {111, 193, 228, 255},
    {121, 214, 154, 255}, {118, 189, 223, 255}, {224, 114, 197, 255},
    {226, 115, 191, 255}, {216, 149, 122, 255}, {216, 152, 115, 255},
    {118, 219, 149, 255}, {135, 182, 207, 255}, {119, 190, 219, 255},
    {127, 211, 149, 255}, {113, 224, 148, 255}, {111, 193, 227, 255},
    {185, 171, 132, 255}, {219, 120, 193, 255}, {224, 115, 194, 255},
    {110, 227, 145, 255}, {222, 117, 194, 255}, {229, 111, 195, 255},
    {205, 132, 192, 255}, {198, 157, 137, 255}, {221, 146, 118, 255},
    {188, 158, 156, 255}, {118, 189, 222, 255}, {223, 115, 197, 255},
    {115, 222, 147, 255}, {221, 146, 121, 255}, {112, 226, 143, 255},
    {221, 116, 196, 255}, {109, 194, 227, 255}, {114, 192, 224, 255},
    {227, 111, 199, 255}, {232, 108, 195, 255}, {128, 189, 204, 255},
    {123, 193, 201, 255}, {123, 215, 146, 255}, {113, 193, 224, 255},
    {115, 191, 223, 255}, {115, 224, 142, 255}, {142, 193, 162, 255},
    {115, 193, 221, 255}, {112, 193, 224, 255}, {204, 135, 186, 255},
    {115, 190, 226, 255}, {223, 116, 194, 255}, {220, 146, 120, 255},
    {224, 145, 115, 255}, {114, 191, 226, 255}, {108, 195, 227, 255},
    {116, 190, 224, 255}, {223, 116, 195, 255}, {114, 222, 148, 255},
    {223, 115, 196, 255}, {109, 228, 145, 255}, {114, 192, 225, 255},
    {117, 219, 149, 255}, {215, 123, 194, 255}, {222, 148, 113, 255},
    {224, 143, 120, 255}, {228, 111, 195, 255}, {117, 222, 142, 255},
    {131, 208, 150, 255}, {110, 194, 226, 255}, {117, 220, 148, 255},
    {146, 187, 170, 255}, {219, 120, 191, 255}, {112, 224, 148, 255},
    {144, 176, 202, 255}};
const int32_t xg_color_palette_length =
    sizeof(xg_color_palette) / sizeof(xg_color);
