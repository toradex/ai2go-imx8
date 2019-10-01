// Copyright (c) 2019 Xnor.ai, Inc.
//
#ifndef __COMMON_UTIL_FILE_H__
#define __COMMON_UTIL_FILE_H__

#include <stdbool.h>
#include <stdint.h>

// Reads an entire file into a data buffer all at once. If it fails, it will
// return false, and data_out and size_out will be untouched.
bool read_entire_file(const char* image_filename, uint8_t** data_out,
                      int32_t* size_out);

enum color_depth {
  kColorDepthRGB,
  kColorDepth1Bit,
};
// Write an uncompressed TGAv1 file to @filename. Data in @image_data is
// interpreted according to @color_depth. @width and @height give the pixel
// dimensions of the image, while @stride gives the byte offset from row to row
// (e.g. for tightly packed RGB, this is 3 * width)
bool write_tga_file(const char* filename, const uint8_t* image_data,
                    enum color_depth color_depth, int32_t width, int32_t height,
                    int32_t stride);

#endif  // __COMMON_UTIL_FILE_H__
