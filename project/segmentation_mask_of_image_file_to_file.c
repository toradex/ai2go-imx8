// Copyright (c) 2019 Xnor.ai, Inc.
//
// This sample runs a segmentation model over an input jpeg and saves the
// resulting mask to a TGA.
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// File-related helpers
#include "common_util/file.h"
// Definitions for the Xnor model API
#include "xnornet.h"

// Returns a mask identifying the precisely bounded area of the image
// representing by a particular class, using deep learning.
// If the model returns more than one segmentation mask, only uses the first.
xnor_evaluation_result* segment_jpeg_using_xnornet(
    const char* filename, xnor_segmentation_mask* mask_out);

// Helper that replaces the ".jpg" or ".jpeg" of a file name with ".class.tga",
// where class is the given class_label
char* make_tga_filename(const char* original_jpeg_name,
                        const char* class_label);

int main(int argc, char* argv[]) {
  if (argc == 2) {
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
      fprintf(stderr, "Usage: %s <image.jpg>\n", argv[0]);
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "Usage: %s <image.jpg>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char* filename = argv[1];
  if (access(filename, R_OK)) {
    fprintf(stderr, "Error: Cannot read file %s\n", filename);
    return EXIT_FAILURE;
  }

  xnor_evaluation_result* result = NULL;
  xnor_segmentation_mask mask;
  if ((result = segment_jpeg_using_xnornet(filename, &mask)) == NULL) {
    return EXIT_FAILURE;
  }

  char* new_filename = make_tga_filename(filename, mask.class_label.label);
  if (new_filename == NULL) {
    xnor_evaluation_result_free(result);
    return EXIT_FAILURE;
  }

  if (write_tga_file(new_filename, mask.bitmap.data, kColorDepth1Bit,
                     mask.bitmap.width, mask.bitmap.height,
                     mask.bitmap.stride)) {
    printf("Saved segmentation mask for '%s' to '%s'\n", mask.class_label.label,
           new_filename);
  } else {
    xnor_evaluation_result_free(result);
    free(new_filename);
    return EXIT_FAILURE;
  }

  xnor_evaluation_result_free(result);
  free(new_filename);
  return EXIT_SUCCESS;
}

void fputs_and_free_error(xnor_error* error) {
  fputs(xnor_error_get_description(error), stderr);
  xnor_error_free(error);
}

xnor_evaluation_result* segment_jpeg_using_xnornet(
    const char* image_filename, xnor_segmentation_mask* mask_out) {
  // Make sure we got a JPEG
  const char* image_ext = strrchr(image_filename, '.');
  if (strcasecmp(image_ext, ".jpg") != 0 &&
      strcasecmp(image_ext, ".jpeg") != 0) {
    fprintf(stderr, "Sorry, this demo only supports jpeg images!\n");
    return NULL;
  }

  // Read the JPEG into memory
  uint8_t* jpeg_data;
  int32_t data_size;
  if (!read_entire_file(image_filename, &jpeg_data, &data_size)) {
    fprintf(stderr, "Couldn't read data from %s!\n", image_filename);
    return NULL;
  }

  // Create the input handle for the Xnornet model.
  xnor_error* error = NULL;
  xnor_input* input = NULL;
  if ((error = xnor_input_create_jpeg_image(jpeg_data, data_size,
                                            &input)) != NULL) {
    fputs_and_free_error(error);
    free(jpeg_data);
    return NULL;
  }

  // Initialize the Xnornet model
  xnor_model* model = NULL;
  if ((error = xnor_model_load_built_in("", NULL, &model)) != NULL) {
    fputs_and_free_error(error);
    xnor_input_free(input);
    free(jpeg_data);
    return NULL;
  }

  // Evaluate the model! (The model looks for known objects in the image, using
  // deep learning)
  xnor_evaluation_result* result = NULL;
  if ((error = xnor_model_evaluate(model, input, NULL, &result)) != NULL) {
    fputs_and_free_error(error);
    xnor_input_free(input);
    xnor_model_free(model);
    free(jpeg_data);
    return NULL;
  }

  // Don't need to keep around the image data any more, now that the model has
  // used it.
  xnor_input_free(input);
  free(jpeg_data);

  // And, since we're done evaluating the model, free the model too
  xnor_model_free(model);

  // Check what kind of model this is by investigating the kind of results it
  // returned.
  // A Segmentation model should always return one or more Segmentation Masks
  if (xnor_evaluation_result_get_type(result) !=
      kXnorEvaluationResultTypeSegmentationMasks) {
    fputs("Oops! I wasn't linked with a segmentation model!\n", stderr);
    xnor_evaluation_result_free(result);
    return NULL;
  }

  int32_t num_masks =
      xnor_evaluation_result_get_segmentation_masks(result, mask_out, 1);
  if (num_masks < 1) {
    fputs("Couldn't get any masks from the model!\n", stderr);
    xnor_evaluation_result_free(result);
    return NULL;
  }

  return result;
}

char* make_tga_filename(const char* filename, const char* class_label) {
  // + 2: one for the extra . and one for the NUL terminator
  int64_t new_filename_len = strlen(filename) + strlen(class_label) + 2;

  char* new_filename = malloc(new_filename_len);
  if (new_filename == NULL) {
    perror("Out of memory");
    return NULL;
  }
  const char* filename_ext = strrchr(filename, '.');
  if (filename_ext == NULL) {
    fprintf(stderr, "Filename doesn't have an extension?\n");
    return NULL;
  }
  if (snprintf(new_filename, new_filename_len, "%.*s.%s.tga",
               (int)(filename_ext - filename), filename, class_label) < 0) {
    fprintf(stderr, "snprintf failed for some reason!\n");
    return NULL;
  }
  return new_filename;
}
