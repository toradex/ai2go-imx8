// Copyright (c) 2019 Xnor.ai, Inc.
//
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common_util/colors.h"
#include "common_util/gstreamer_video_pipeline.h"
#include "common_util/overlays.h"
#include "xnornet.h"

static xg_color color_by_id(int32_t id)
{
	return xg_color_palette[id % xg_color_palette_length];
}

int main(int argc, char *argv[])
{
	// Forward declare variables we may need to clean up later
	xnor_model *model = NULL;
	xnor_error *error = NULL;
	xg_pipeline *pipeline = NULL;
	xg_frame *frame = NULL;
	xnor_input *input = NULL;
	xnor_evaluation_result *result = NULL;

	if (argc > 1)
	{
		if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))
		{
			fprintf(stderr, "Usage: %s <gst_flags> <gtk_flags>\n", argv[0]);
			return EXIT_FAILURE;
		}
	}
	// Allow the video pipeline to parse the arguments, we will be ignoring them
	xg_init(&argc, &argv);

	// Load the Xnor model to get a model handle. We will free this at the end of
	// main(), either via a successful return or after the fail: label.
	error = xnor_model_load_built_in("", NULL, &model);
	if (error != NULL)
	{
		fprintf(stderr, "%s\n", xnor_error_get_description(error));
		goto fail;
	}

	// Get the model information
	xnor_model_info model_info;
	model_info.xnor_model_info_size = sizeof(model_info);
	error = xnor_model_get_info(model, &model_info);
	if (error != NULL)
	{
		fprintf(stderr, "%s\n", xnor_error_get_description(error));
		goto fail;
	}

	// Make sure that the model is actually a classification model. If you see
	// this, it means you should either switch which model is listed in the
	// Makefile, or run one of the other demos.
	if (model_info.result_type != kXnorEvaluationResultTypeClassLabels)
	{
		fprintf(stderr, "%s is not a classification model! This sample "
				"requires a classification model to be installed (e.g. "
				"facial-expression-classifier).",
			model_info.name);
		goto fail;
	}

	puts("Xnor Live Scene Classification Demo");
	printf("Model: %s\n", model_info.name);
	printf("  version '%s'\n", model_info.version);

	// Set up the video pipeline. The argument to this function is the title that
	// goes in the title bar of the window, see gstreamer_video_pipeline.h for
	// more information.

	if (argc == 1)
		pipeline = xg_create_video_overlay_pipeline(
				"Xnor Object Detection Demo", "/dev/video0", true);
	else
		pipeline = xg_create_video_overlay_pipeline(
				"Xnor Object Detection Demo", argv[1], true);

	if (pipeline == NULL)
	{
		fputs("Couldn't create video pipeline\n", stderr);
		goto fail;
	}

	// Start up the video pipeline (this opens the window and starts polling the
	// video input device).
	xg_pipeline_start(pipeline);

	// xg_pipeline_running() will return true until the window is closed
	while (xg_pipeline_running(pipeline))
	{
		// Retrieves the last video frame from the pipeline. These are not
		// necessarily sequential, e.g. if inference is running slower than the
		// video input device. The pipeline will handle dropping intermediate frames
		// so that this call always gets the most recent one.
		frame = xg_pipeline_get_frame(pipeline);
		// NULL frame can mean the pipeline stopped in the middle of the above call,
		// so just break out of the loop now.
		if (frame == NULL)
		{
			break;
		}

		// Create a handle so we can pass the input frame to the Xnor model
		error = xnor_input_create_rgb_image(frame->width, frame->height,
						    frame->data, &input);
		if (error != NULL)
		{
			fprintf(stderr, "%s\n", xnor_error_get_description(error));
			goto fail;
		}

		// Call the model! This is where the magic happens.
		error = xnor_model_evaluate(model, input, NULL, &result);
		if (error != NULL)
		{
			fprintf(stderr, "%s\n", xnor_error_get_description(error));
			goto fail;
		}

		xg_pipeline_clear_overlays(pipeline);

		// Ask how many class labels there were, then allocate enough memory to
		// hold them all
		int32_t num_class_labels =
		    xnor_evaluation_result_get_class_labels(result, NULL, 0);
		xnor_class_label *classes =
		    calloc(num_class_labels, sizeof(xnor_class_label));
		if (classes == NULL)
		{
			fputs("Couldn't allocate memory for class labels\n", stderr);
			goto fail;
		}

		// Get the label data and display the results
		xnor_evaluation_result_get_class_labels(result, classes,
							num_class_labels);
		// Approximate height of the overlay text as a fraction of the 1080p frame
		const float overlay_line_height = OVERLAY_TEXT_SIZE * 1.5f / frame->height;
		for (int32_t i = 0; i < num_class_labels; ++i)
		{
			xg_overlay *text =
			    xg_overlay_create_text(0, i * overlay_line_height, classes[i].label,
						   color_by_id(classes[i].class_id));
			xg_pipeline_add_overlay(pipeline, text);
		}

		// Clean up after the frame-specific stuff
		free(classes);
		xnor_evaluation_result_free(result);
		xnor_input_free(input);
		xg_frame_free(frame);
		// Set the variables to NULL so we don't double-free if we jump to fail:
		result = NULL;
		input = NULL;
		frame = NULL;
	}

	xg_pipeline_free(pipeline);
	xnor_model_free(model);
	return EXIT_SUCCESS;
fail:
	if (pipeline && xg_pipeline_running(pipeline))
	{
		xg_pipeline_stop(pipeline);
	}
	// If any of these are NULL, the corresponding free() function will do nothing
	xg_frame_free(frame);
	xg_pipeline_free(pipeline);
	xnor_error_free(error);
	xnor_input_free(input);
	xnor_model_free(model);
	xnor_evaluation_result_free(result);
	return EXIT_FAILURE;
}
