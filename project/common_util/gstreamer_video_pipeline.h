// Copyright (c) 2019 Xnor.ai, Inc.
//
#ifndef __COMMON_UTIL_GSTREAMER_VIDEO_PIPELINE_H__
#define __COMMON_UTIL_GSTREAMER_VIDEO_PIPELINE_H__

#include <stdbool.h>
#include <stdint.h>

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#include "overlays.h"

////// matheus.castello
#define DEMO_HEIGH 720;
#define DEMO_WIDTH 576;

typedef struct xg_error
{
	const char *message;
	struct xg_error *next;
} xg_error;

struct xg_pipeline
{
	bool running;
	GtkWindow *window;

	GstPipeline *gst_pipeline;
	GstBus *bus;
	GstElement *appsink;
	GtkWidget *video_widget;
	GstVideoOverlay *overlay;
	// Prevent querying appsink while stream stopping
	pthread_mutex_t gst_pipeline_lock;

	xg_overlay *overlay_list;
	// Prevent drawing overlays while updating the list
	pthread_mutex_t overlay_lock;

	bool error_occurred;
};
//////// matheus.castello

// Xnor-sample Gstreamer pipeline
typedef struct xg_pipeline xg_pipeline;

// Xnor-sample Gstreamer video frame.
typedef struct xg_frame
{
	// A string specifying the video format, e.g. "RGB"
	const char *format;
	// Dimensions of the frame, in pixels
	int32_t width, height;
	// Raw frame data. Structure is dictated by @format
	uint8_t *data;
} xg_frame;

// Must be called exactly once at the start of the program
void xg_init(int *argc, char **argv[]);

// Initializes a GStreamer pipeline. The pipeline opens a window which displays
// a live video feed and draws arbitrary overlays on top of the video. You must
// free the pipeline with xg_pipeline_free when you are done with it.
xg_pipeline *xg_create_video_overlay_pipeline(const char *window_title,
						const char *device, bool gui);
// Starts the pipeline and opens the window
void xg_pipeline_start(xg_pipeline *pipeline);
// Returns whether or not the pipeline has been stopped (e.g. by a keyboard
// event or WM message)
bool xg_pipeline_running(xg_pipeline *pipeline);
// Extracts the latest frame of video from the pipeline. You must free this
// frame using xg_frame_free() when done with it.
xg_frame *xg_pipeline_get_frame(xg_pipeline *pipeline);
// Adds an overlay to the pipeline, which will be drawn on top of the video each
// frame until cleared. An overlay can only be added to one pipeline at a time,
// and the pipeline takes ownership of the overlay's resources when it is added.
void xg_pipeline_add_overlay(xg_pipeline *pipeline, xg_overlay *overlay);
// Removes all overlays from the pipeline, freeing them
void xg_pipeline_clear_overlays(xg_pipeline *pipeline);
// Toggles between paused and playing states
void xg_pipeline_toggle_pause(xg_pipeline *pipeline);
// Stops the pipeline, hiding the window
void xg_pipeline_stop(xg_pipeline *pipeline);
// Frees resources and disconnects signal handlers for the pipeline.
void xg_pipeline_free(xg_pipeline *pipeline);

// Frees resources for a video frame
void xg_frame_free(xg_frame *frame);

#endif // __COMMON_UTIL_GSTREAMER_VIDEO_PIPELINE_H__
