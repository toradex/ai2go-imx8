// Copyright (c) 2019 Xnor.ai, Inc.
//
// This sample runs a detection model over an input image and prints out the
// resulting detected object locations as a JSON document. (Useful for e.g.
// reading results from another application or over HTTP).
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// File-loading helpers
#include "common_util/file.h"
// Definitions for the Xnor model API
#include "xnornet.h"

struct input {
  xnor_input* xnor_input;
  uint8_t* input_image_data;
};
// Load the image at @image_filename and pass it to the model to create an input
// handle.  Returned as a struct so that we can free the allocated image data
// after the inference completes.
struct input input_create_from_jpeg(const char* image_filename);

// This variable specifies whether to tabify and linebreak when printing the
// JSON document. It could also be a command line argument, but using a
// preprocessor variable lets us avoid running some of the code altogether.
#define JSON_PRETTY true
// Print a JSON representation of the bounding boxes in @boxes to @dest,
// starting at the indentation level given by @indentlevel
// If JSON_PRETTY is false, no indentation or newlines are printed and
// @indentlevel is ignored.
void json_dump_bounding_boxes(xnor_bounding_box* boxes, int32_t num_boxes,
                              FILE* dest, int32_t indentlevel);

int main(int argc, char* argv[]) {
  if (argc == 2) {
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
      fprintf(stderr, "Usage: %s <image.jpg> > output.json\n", argv[0]);
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "Usage: %s <image.jpg> > output.json\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char* filename = argv[1];
  if (access(filename, R_OK)) {
    fprintf(stderr, "Error: Cannot read file %s\n", filename);
    return EXIT_FAILURE;
  }

  struct input input;
  if ((input = input_create_from_jpeg(filename)).xnor_input == NULL) {
    return EXIT_FAILURE;
  }

  // Initialize the Xnornet model
  xnor_model* model = NULL;
  xnor_error* error = NULL;
  if ((error = xnor_model_load_built_in("", NULL, &model)) != NULL) {
    fputs(xnor_error_get_description(error), stderr);
    xnor_error_free(error);
    xnor_input_free(input.xnor_input);
    free(input.input_image_data);
    return EXIT_FAILURE;
  }

  // Get the model information
  xnor_model_info model_info;
  model_info.xnor_model_info_size = sizeof(model_info);
  error = xnor_model_get_info(model, &model_info);
  if (error != NULL) {
    fprintf(stderr, "%s\n", xnor_error_get_description(error));
    xnor_error_free(error);
    xnor_model_free(model);
    xnor_input_free(input.xnor_input);
    free(input.input_image_data);
    return false;
  }

  // Make sure that the model is actually an object detection model. If you
  // see this, it means you should either switch which model is listed in the
  // Makefile, or run one of the other demos.
  if (model_info.result_type != kXnorEvaluationResultTypeBoundingBoxes) {
    fprintf(stderr, "%s is not a detection model! This sample "
            "requires a detection model to be installed (e.g. "
            "person-pet-vehicle-detector).", model_info.name);
    xnor_model_free(model);
    xnor_input_free(input.xnor_input);
    free(input.input_image_data);
    return false;
  }

  // Evaluate the model! (The model looks for known objects in the image, using
  // deep learning)
  xnor_evaluation_result* result = NULL;
  if ((error = xnor_model_evaluate(model, input.xnor_input, NULL, &result)) !=
      NULL) {
    fputs(xnor_error_get_description(error), stderr);
    xnor_error_free(error);
    xnor_model_free(model);
    xnor_input_free(input.xnor_input);
    free(input.input_image_data);
    return EXIT_FAILURE;
  }

  // Don't need to keep around the image data any more, now that the model has
  // used it.
  xnor_input_free(input.xnor_input);
  free(input.input_image_data);

  // And, since we're done evaluating the model, free the model too
  xnor_model_free(model);

  // Dynamically allocate enough space to hold all of the returned bounding
  // boxes
  int32_t num_objects =
      xnor_evaluation_result_get_bounding_boxes(result, NULL, 0);
  xnor_bounding_box* objects = malloc(sizeof(xnor_bounding_box) * num_objects);
  xnor_evaluation_result_get_bounding_boxes(result, objects, num_objects);

  json_dump_bounding_boxes(objects, num_objects, stdout, 0);
  fputs("\n", stdout);

  xnor_evaluation_result_free(result);
  free(objects);

  return EXIT_SUCCESS;
}

struct input input_create_from_jpeg(const char* image_filename) {
  // Make sure we got a JPEG
  const char* image_ext = strrchr(image_filename, '.');
  if (strcasecmp(image_ext, ".jpg") != 0 &&
      strcasecmp(image_ext, ".jpeg") != 0) {
    fprintf(stderr, "Sorry, this demo only supports jpeg images!\n");
    return (struct input){NULL};
  }

  // Read the JPEG into memory
  uint8_t* jpeg_data;
  int32_t data_size;
  if (!read_entire_file(image_filename, &jpeg_data, &data_size)) {
    fprintf(stderr, "Couldn't read data from %s!\n", image_filename);
    return (struct input){NULL};
  }

  // Create the input handle for the Xnornet model.
  xnor_error* error = NULL;
  xnor_input* input = NULL;
  if ((error = xnor_input_create_jpeg_image(jpeg_data, data_size, &input)) !=
      NULL) {
    fputs(xnor_error_get_description(error), stderr);
    free(jpeg_data);
    return (struct input){NULL};
  }

  return (struct input){input, jpeg_data};
}

#if JSON_PRETTY
#define JSON_PRETTY_NEWLINE "\n"
#define JSON_PRETTY_INDENT "  "
#else  // JSON_PRETTY
#define JSON_PRETTY_NEWLINE ""
#define JSON_PRETTY_INDENT ""
#endif  // JSON_PRETTY

void do_indent(FILE* dest, int32_t indentlevel) {
#if JSON_PRETTY
  for (int32_t i = 0; i < indentlevel; ++i) {
    fputs(JSON_PRETTY_INDENT, dest);
  }
#endif  // JSON_PRETTY
}

void json_dump_rectangle(xnor_rectangle rect, FILE* dest, int32_t indentlevel) {
  do_indent(dest, indentlevel);
  fputs("{" JSON_PRETTY_NEWLINE, dest);

  do_indent(dest, indentlevel + 1);
  fprintf(dest, "\"x\": %f," JSON_PRETTY_NEWLINE, rect.x);
  do_indent(dest, indentlevel + 1);
  fprintf(dest, "\"y\": %f," JSON_PRETTY_NEWLINE, rect.y);
  do_indent(dest, indentlevel + 1);
  fprintf(dest, "\"width\": %f," JSON_PRETTY_NEWLINE, rect.width);
  do_indent(dest, indentlevel + 1);
  fprintf(dest, "\"height\": %f" JSON_PRETTY_NEWLINE, rect.height);

  do_indent(dest, indentlevel);
  fputs("}", dest);
}

void json_dump_class_label(xnor_class_label label, FILE* dest,
                           int32_t indentlevel) {
  do_indent(dest, indentlevel);
  fputs("{" JSON_PRETTY_NEWLINE, dest);

  do_indent(dest, indentlevel + 1);
  fprintf(dest, "\"class_id\": %d," JSON_PRETTY_NEWLINE, label.class_id);
  do_indent(dest, indentlevel + 1);
  fprintf(dest, "\"label\": %s" JSON_PRETTY_NEWLINE, label.label);

  do_indent(dest, indentlevel);
  fputs("}", dest);
}

void json_dump_bounding_box(xnor_bounding_box box, FILE* dest,
                            int32_t indentlevel) {
  do_indent(dest, indentlevel);
  fputs("{" JSON_PRETTY_NEWLINE, dest);

  json_dump_class_label(box.class_label, dest, indentlevel + 1);
  fputs("," JSON_PRETTY_NEWLINE, dest);
  json_dump_rectangle(box.rectangle, dest, indentlevel + 1);
  fputs(JSON_PRETTY_NEWLINE, dest);

  do_indent(dest, indentlevel);
  fputs("}", dest);
}

void json_dump_bounding_boxes(xnor_bounding_box* boxes, int32_t num_boxes,
                              FILE* dest, int32_t indentlevel) {
  do_indent(dest, indentlevel);
  fputs("[", dest);
  if (num_boxes > 0) {
    fputs(JSON_PRETTY_NEWLINE, dest);
  }
  for (int32_t i = 0; i < num_boxes; ++i) {
    json_dump_bounding_box(boxes[i], dest, indentlevel + 1);
    if (i < num_boxes - 1) {
      fputs("," JSON_PRETTY_NEWLINE, dest);
    } else {
      fputs(JSON_PRETTY_NEWLINE, dest);
    }
  }
  do_indent(dest, indentlevel);
  fputs("]", dest);
}
