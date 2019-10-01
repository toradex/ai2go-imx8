// Copyright (c) 2019 Xnor.ai, Inc.
//
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#include "common_util/file.h"
#include "xnornet.h"

void panic_on_error(xnor_error* error) {
  if (error != NULL) {
    fprintf(stderr, "Unhandled error: %s\n", xnor_error_get_description(error));
    xnor_error_free(error);
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char* argv[]) {
  xnor_model* model;
  panic_on_error(xnor_model_load_built_in(NULL, NULL, &model));

  const char* filename = "../../test-images/dog.jpg";
  if (argc >= 2) {
    filename = argv[1];
  }

  uint8_t* dog_jpeg;
  int32_t dog_jpeg_length;
  if (!read_entire_file(filename, &dog_jpeg,
                        &dog_jpeg_length)) {
    fprintf(stderr, "Failed to load %s\n", filename);
    return EXIT_FAILURE;
  }

  xnor_input* input;
  panic_on_error(
      xnor_input_create_jpeg_image(dog_jpeg, dog_jpeg_length, &input));

  xnor_evaluation_result* result;
  panic_on_error(xnor_model_evaluate(model, input, NULL, &result));

#define MAX_BOXES 10
  xnor_bounding_box boxes[MAX_BOXES];
  int num_boxes = xnor_evaluation_result_get_bounding_boxes(result, boxes, MAX_BOXES);
  if (num_boxes > MAX_BOXES) {
      /* if there are more than MAX_BOXES boxes,
        xnor_evaluation_result_get_bounding_boxes will still return the
        total number of boxes, so we clamp it down to our maximum */
      num_boxes = MAX_BOXES;
  }
  if (num_boxes < 0) {
      /* An error occurred! Maybe this wasn't an object detection model? */
      fputs("Error: Not an object detection model\n", stderr);
      return EXIT_FAILURE;
  }
  for (int i = 0; i < num_boxes; ++i) {
      printf("I spy with my little eye a %s\n", boxes[i].class_label.label);
  }

  xnor_input_free(input);
  free(dog_jpeg);  /* Note: dog_jpeg must be freed AFTER input is freed! */
  xnor_evaluation_result_free(result);
  xnor_model_free(model);

  return EXIT_SUCCESS;
}
