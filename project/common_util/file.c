// Copyright (c) 2019 Xnor.ai, Inc.
//
#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool read_entire_file(const char* filename, uint8_t** data_out,
                      int32_t* size_out) {
  FILE* file = fopen(filename, "rb");
  if (file == NULL) {
    perror("Error opening file");
    return false;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    perror("Error finding end of file");
    return false;
  }
  int32_t file_size = ftell(file);
  if (fseek(file, 0, SEEK_SET) != 0) {
    perror("Error finding beginning of file");
    return false;
  }
  uint8_t* data = (uint8_t*)malloc(file_size);
  if (data == NULL) {
    perror("Error allocating data to read file");
    return false;
  }
  int32_t read = fread(data, 1, file_size, file);
  if (read < file_size) {
    if (ferror(file)) {
      perror("Error reading file");
    }
    if (feof(file)) {
      fprintf(stderr, "Unexpected short read: %d < %d\n", read, file_size);
    }
    free(data);
    return false;
  }
  fclose(file);
  *data_out = data;
  *size_out = file_size;
  return true;
}

#pragma pack(push, 1)
struct tga_header {
  uint8_t IDLength;
  uint8_t ColorMapType;
  uint8_t ImageType;
  uint16_t ColorMapStart;
  uint16_t ColorMapLength;
  uint8_t ColorMapDepth;
  uint16_t XOffset;
  uint16_t YOffset;
  uint16_t Width;
  uint16_t Height;
  uint8_t PixelDepth;
  uint8_t ImageDescriptor;
};
#pragma pack(pop)

enum tga_image_type {
  kTgaImageTypeNoImage = 0,
  kTgaImageTypeColormapped = 1,
  kTgaImageTypeTruecolor = 2,
  kTgaImageTypeMonochrome = 3,

  kTgaColorMapFlagRleEncoded = 8,

  kTgaImageTypeColormappedEncoded = 9,
  kTgaImageTypeTruecolorEncoded = 10,
  kTgaImageTypeMonoChromeEncoded = 11,
};

enum tga_image_descriptor {
  kTgaImageOriginLeft   = 0 << 4,
  kTgaImageOriginRight  = 1 << 4,
  kTgaImageOriginBottom = 0 << 5,
  kTgaImageOriginTop    = 1 << 5,
};

bool write_tga_file(const char* filename, const uint8_t* image_data,
                    enum color_depth color_depth, int32_t width, int32_t height,
                    int32_t stride) {
  if (width < 0 || height < 0) {
    fputs("Unexpected negative width/height!\n", stderr);
    return false;
  }
  if (color_depth == kColorDepth1Bit && stride < width / 8) {
    fputs("Unexpected short stride!\n", stderr);
    return false;
  }

  FILE* file = fopen(filename, "wb");
  if (file == NULL) {
    perror("Error opening file");
    return false;
  }

  enum tga_image_type image_type = kTgaImageTypeNoImage;
  int16_t pixel_depth = 1;
  switch (color_depth) {
    case kColorDepthRGB: {
      image_type = kTgaImageTypeColormapped;
      pixel_depth = 24;
      break;
    }
    case kColorDepth1Bit: {
      image_type = kTgaImageTypeMonochrome;
      pixel_depth = 8;
      break;
    }
  }

  struct tga_header header = {
    .IDLength = 0,
    .ImageType = image_type,
    .XOffset = 0,
    .YOffset = 0,
    .Width = width,
    .Height = height,
    .PixelDepth = pixel_depth,
    .ImageDescriptor = kTgaImageOriginLeft | kTgaImageOriginTop,
  };

  if (fwrite(&header, sizeof(header), 1, file) != 1) {
    perror("Error writing tga header");
    fclose(file);
    return false;
  }

  if (color_depth == kColorDepth1Bit) {
    for (int32_t y = 0; y < height; ++y) {
      for (int32_t x = 0; x < width; ++x) {
        int32_t byte_i = y * stride + x / 8;
        int32_t bit_i = x % 8;
        bool val = (image_data[byte_i] >> bit_i) & 0x1;
        uint8_t channel_val = val ? 255 : 0;
        if (fwrite(&channel_val, 1, 1, file) != 1) {
          perror("Error writing tga data");
          fclose(file);
          return false;
        }
      }
    }
  } else {
    for (int32_t y = 0; y < height; ++y) {
      for (int32_t x = 0; x < width; ++x) {
        uint8_t rgb[3];
        memcpy(rgb, image_data + (y * stride + x * 3), 3);
        if (fwrite(rgb, 3, 1, file) != 1) {
          perror("Error writing tga data");
          fclose(file);
          return false;
        }
      }
    }
  }

  fclose(file);
  return true;
}
