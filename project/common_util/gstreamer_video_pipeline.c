// Copyright (c) 2019 Xnor.ai, Inc.
//
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib-object.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gtk/gtk.h>
#include <pthread.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#include "gstwaylandsink.h"
#include "wayland.h"
#else
#error "Wayland is not supported in GTK+"
#endif

#include "gstreamer_video_pipeline.h"

static void on_destroy_event(GtkWidget *widget, gpointer user_data);

///////////////////
// COLLABORA
//////////////////
static void
build_window(xg_pipeline *pipeline)
{
	GtkBuilder *builder;
	GError *error = NULL;

	builder = gtk_builder_new();
	if (!gtk_builder_add_from_file(builder, "window.ui", &error))
	{
		g_error("Failed to load window.ui: %s", error->message);
		g_error_free(error);
		goto exit;
	}

	pipeline->window = GTK_WINDOW(gtk_builder_get_object(builder, 
				"window"));
	g_object_ref(pipeline->window);
	g_signal_connect(pipeline->window, "destroy",
				G_CALLBACK(on_destroy_event), pipeline);

	pipeline->video_widget = GTK_WIDGET(gtk_builder_get_object(builder,
					"videoarea"));

exit:
	g_object_unref(builder);
}

///////////////////
// Error handling
///////////////////

// Print out a message to stderr and exit with EXIT_FAILURE
// Use only if error is unrecoverable.
static noreturn void panic(const char *msg)
{
	fprintf(stderr, "PANIC: %s\n", msg);
	exit(EXIT_FAILURE);
}

// Prints formatted message to stderr with an appropriate prefix indicating it
// is an error and sets a flag on pipeline indicating that an error occurred.
static void errorf(xg_pipeline *pipeline, const char *fmt, ...)
{
	pipeline->error_occurred = true;
	fputs("ERROR: ", stderr);
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputs("\n", stderr);
}

static void acquire_mutex_or_die(pthread_mutex_t *mutex)
{
	if (pthread_mutex_lock(mutex) != 0)
	{
		// pthread_mutex_lock returns non-zero in the following cases:
		//  - EINVAL: mutex is protected or uninitialized
		//  - EAGAIN: too many recursive locks
		//  - EDEADLK: current thread already owns it
		// These all indicate bugs in the program rather than possible failure
		// cases, so we should simply panic now.
		panic("Couldn't acquire a required lock");
	}
}

static void release_mutex_or_die(pthread_mutex_t *mutex)
{
	if (pthread_mutex_unlock(mutex) != 0)
	{
		// pthread_mutex_unlock returns non-zero in the following cases:
		//  - EINVAL: mutex is uninitialized
		//  - EAGAIN: too many recursive locks
		//  - EPERM: current thread does not own the mutex
		// These all indicate bugs in the program rather than possible failure
		// cases, so we should simply panic now.
		panic("Couldn't release a required lock");
	}
}

//////////////
// Callbacks
//////////////

static void draw_overlays(GstElement *gst_overlay, cairo_t *cr,
			  guint64 timestamp, guint64 duration,
			  gpointer user_data)
{
	xg_pipeline *pipeline = (xg_pipeline *)user_data;

	acquire_mutex_or_die(&pipeline->overlay_lock);
	// Got the lock, now draw the overlays
	cairo_surface_t *surface = cairo_get_target(cr);
	if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE)
	{
		errorf(pipeline, "Unexpected surface type %d when drawing overlays",
		       cairo_surface_get_type(surface));
		release_mutex_or_die(&pipeline->overlay_lock);
		return;
	}
	int32_t surface_width = cairo_image_surface_get_width(surface);
	int32_t surface_height = cairo_image_surface_get_height(surface);

	xg_overlay *overlay = pipeline->overlay_list;
	while (overlay != NULL)
	{
		xg_overlay_draw(overlay, cr, surface_width, surface_height);
		overlay = overlay->next;
	}
	release_mutex_or_die(&pipeline->overlay_lock);
}

static gboolean on_key_press_event(GtkWidget *widget, GdkEvent *event,
				   gpointer user_data)
{
	xg_pipeline *pipeline = (xg_pipeline *)user_data;
	if (event->type != GDK_KEY_PRESS)
	{
		return false;
	}
	switch (event->key.keyval)
	{
	case GDK_KEY_Escape:
	case GDK_KEY_q:
	case GDK_KEY_Q:
		xg_pipeline_stop(pipeline);
		break;
	case GDK_KEY_p:
	case GDK_KEY_space:
		xg_pipeline_toggle_pause(pipeline);
		break;
	}
	return true;
}

static void on_destroy_event(GtkWidget *widget, gpointer user_data)
{
	xg_pipeline *pipeline = (xg_pipeline *)user_data;
	xg_pipeline_stop(pipeline);
}

static void on_bus_message(GstBus *bus, GstMessage *message,
			   gpointer user_data)
{
	xg_pipeline *pipeline = (xg_pipeline *)user_data;
	switch (GST_MESSAGE_TYPE(message))
	{
	case GST_MESSAGE_EOS:
	{
		xg_pipeline_stop(pipeline);
		break;
	}
	case GST_MESSAGE_ERROR:
	{
		GError *err = NULL;
		gchar *debug_info = NULL;
		gst_message_parse_error(message, &err, &debug_info);
		fprintf(stderr, "ERROR in %s: %s\ndebug:%s\n",
			GST_MESSAGE_SRC_NAME(message), err->message, debug_info);
		g_error_free(err);
		g_free(debug_info);
		break;
	}

	// Safe to ignore
	case GST_MESSAGE_STATE_CHANGED:
	case GST_MESSAGE_STREAM_STATUS:
	case GST_MESSAGE_STREAM_START:
	case GST_MESSAGE_ASYNC_DONE:
	case GST_MESSAGE_NEW_CLOCK:
		break;

	default:
	{
		fprintf(stderr, "%s: unhandled message: %s\n",
			GST_MESSAGE_SRC_NAME(message),
			GST_MESSAGE_TYPE_NAME(message));
		break;
	}
	}
}

static void on_bus_sync_message(GstBus *bus, GstMessage *message,
				gpointer user_data)
{

	xg_pipeline *pipeline = (xg_pipeline *)user_data;
	const GstStructure *message_struct = gst_message_get_structure(message);

	if (gst_is_wayland_display_handle_need_context_message(message))
	{
		GstContext *context;
		GdkDisplay *display;
		struct wl_display *display_handle;

		display = gtk_widget_get_display(pipeline->video_widget);
		display_handle = gdk_wayland_display_get_wl_display(display);
		context = gst_wayland_display_handle_context_new(display_handle);
		gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(message)), context);
	}
	else if (gst_is_video_overlay_prepare_window_handle_message(message))
	{
		GtkAllocation allocation;
		GdkWindow *window;
		struct wl_surface *window_handle;

		/* GST_MESSAGE_SRC (message) will be the overlay object that we have to
		* use. This may be waylandsink, but it may also be playbin. In the latter
		* case, we must make sure to use playbin instead of waylandsink, because
		* playbin resets the window handle and render_rectangle after restarting
		* playback and the actual window size is lost */
		pipeline->overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message));

		gtk_widget_get_allocation(pipeline->video_widget, &allocation);
		window = gtk_widget_get_window(pipeline->video_widget);
		window_handle = gdk_wayland_window_get_wl_surface(window);

		g_print("setting window handle and size (%d x %d)\n",
			allocation.width, allocation.height);

		gst_video_overlay_set_window_handle(pipeline->overlay, (guintptr)window_handle);
		gst_video_overlay_set_render_rectangle(pipeline->overlay, allocation.x,
						       allocation.y, allocation.width, allocation.height);
	}
}

////////////////////
// Private helpers
////////////////////

static GstElement *make_element(xg_pipeline *pipeline, const char *type,
				const char *name)
{
	GstElement *element = gst_element_factory_make(type, name);
	if (element == NULL)
	{
		errorf(pipeline, "Couldn't create element %s (%s)", name, type);
		return NULL;
	}
	if (!gst_bin_add(GST_BIN(pipeline->gst_pipeline), element))
	{
		gst_object_unref(GST_OBJECT(element));
		errorf(pipeline, "Couldn't add %s to pipeline", name);
		return NULL;
	}
	return element;
}

static void link_elements(xg_pipeline *pipeline, GstElement *src,
			  GstElement *dest)
{
	if (!gst_element_link(src, dest))
	{
		const char *srctype = g_type_name(
		    gst_element_factory_get_element_type(gst_element_get_factory(src)));
		const char *desttype = g_type_name(
		    gst_element_factory_get_element_type(gst_element_get_factory(dest)));
		char *srcname = gst_element_get_name(src);
		char *destname = gst_element_get_name(dest);
		errorf(pipeline, "Couldn't connect elements %s (%s) and %s (%s)", srcname,
		       srctype, destname, desttype);
		g_free(srcname);
		g_free(destname);
	}
}

static GstElement *make_video_source(xg_pipeline *pipeline, const char *device)
{
	printf("Using device %s\n", device);

	// common g_objects
	GstElement *videoflip = 
		make_element(pipeline, "videoflip", "video_mirror");
	g_object_set(G_OBJECT(videoflip), "video-direction", 4, NULL);
	GstElement *jpegdecoder =
		make_element(pipeline, "autovideoconvert", "source_jpegdec");
	GstElement *capsfilter = 
		make_element(pipeline, "capsfilter", "source_capsfilter");
	GstCaps *caps_settings =
		gst_caps_from_string(
			"video/x-raw,width=320,height=240"
		);
	g_object_set(capsfilter, "caps", caps_settings, NULL);

	// mount video 4 linux and pipeline
	GstElement *v4l2src =
		make_element(pipeline, "v4l2src", "source_v4l2src");
	g_object_set(G_OBJECT(v4l2src), "device", device, NULL);
	GstElement *scale =
		make_element(pipeline, "videoscale", "source_videoscale");

	link_elements(pipeline, v4l2src, capsfilter);
	link_elements(pipeline, capsfilter, jpegdecoder);
	link_elements(pipeline, jpegdecoder, videoflip);

	return videoflip;
}

static GstElement *make_app_sink(xg_pipeline *pipeline)
{
	GstElement *queue;
	GstElement *converter;
	GstElement *capsfilter;
	GstElement *appsink;

	queue = make_element(pipeline, "queue", "appsink_queue");
	converter = make_element(pipeline, "videoconvert", "appsink_converter");
	capsfilter = make_element(pipeline, "capsfilter", "appsink_capsfilter");
	appsink = make_element(pipeline, "appsink", "appsink");

	if (pipeline->error_occurred)
	{
		return NULL;
	}

	g_object_set(capsfilter, "caps",
		     gst_caps_from_string("video/x-raw,format=RGB"), NULL);
	gst_app_sink_set_max_buffers(GST_APP_SINK(appsink), 1);
	gst_app_sink_set_drop(GST_APP_SINK(appsink), true);
	link_elements(pipeline, queue, converter);
	link_elements(pipeline, converter, capsfilter);
	link_elements(pipeline, capsfilter, appsink);

	pipeline->appsink = appsink;
	return queue;
}

static void make_overlay(xg_pipeline *pipeline, GstElement **overlay_in,
			 GstElement **overlay_out)
{
	GstElement *queue = NULL;
	GstElement *converter = NULL;
	GstElement *overlay = NULL;
	queue = make_element(pipeline, "queue", "overlay_queue");
	converter = make_element(pipeline, "videoconvert", "overlay_converter");
	overlay = make_element(pipeline, "cairooverlay", "overlay");

	if (pipeline->error_occurred)
	{
		return;
	}

	g_signal_connect(overlay, "draw", G_CALLBACK(draw_overlays), pipeline);
	link_elements(pipeline, queue, converter);
	link_elements(pipeline, converter, overlay);
	*overlay_in = queue;
	*overlay_out = overlay;
}

static GstElement *make_auto_sink(xg_pipeline *pipeline)
{
	GstElement *queue;
	GstElement *converter;
	GstElement *sink;
	queue = make_element(pipeline, "queue", "auto_sink_queue");
	converter = make_element(pipeline, "videoconvert", "auto_sink_converter");
	sink = make_element(pipeline, "waylandsink", "auto_sink");

	// set context
	GdkDisplay *display = gtk_widget_get_display(pipeline->video_widget);
	struct wl_display *display_handle = gdk_wayland_display_get_wl_display(display);
	GstContext *context = gst_wayland_display_handle_context_new(display_handle);
	gst_element_set_context(sink, context);
	GstElementFactory *queue_factory = gst_element_factory_find("waylandsink");
	g_assert(G_IS_OBJECT(queue_factory));

	if (pipeline->error_occurred)
	{
		return NULL;
	}

	link_elements(pipeline, queue, converter);
	link_elements(pipeline, converter, sink);
	return queue;
}

static void build_video_overlay_pipeline(xg_pipeline *pipeline, const char *device)
{
	pipeline->gst_pipeline =
	    GST_PIPELINE(gst_pipeline_new("video-overlay-pipeline"));

	GstElement *source = NULL;
	GstElement *tee_no_overlay = NULL;
	GstElement *app_sink = NULL;
	GstElement *overlay_in = NULL;
	GstElement *overlay_out = NULL;
	GstElement *auto_sink = NULL;

	source = make_video_source(pipeline, device);
	tee_no_overlay = make_element(pipeline, "tee", "tee_no_overlay");
	app_sink = make_app_sink(pipeline);
	make_overlay(pipeline, &overlay_in, &overlay_out);
	auto_sink = make_auto_sink(pipeline);

	if (pipeline->error_occurred)
	{
		return;
	}

	link_elements(pipeline, source, tee_no_overlay);
	link_elements(pipeline, tee_no_overlay, app_sink);
	link_elements(pipeline, tee_no_overlay, overlay_in);
	link_elements(pipeline, overlay_out, auto_sink);
}

static xg_pipeline *xg_create_base_pipeline(const char *window_title)
{
	xg_pipeline *pipeline = calloc(1, sizeof(xg_pipeline));
	if (pipeline == NULL)
	{
		errorf(pipeline, "Couldn't allocate memory for pipeline");
		return NULL;
	}

	// Keep track of whether we are started or stopped
	pipeline->running = false;
	pthread_mutex_init(&pipeline->gst_pipeline_lock, NULL);

	return pipeline;
}

static void xg_base_pipeline_init(xg_pipeline *pipeline)
{
	if (pipeline->appsink == NULL)
	{
		errorf(pipeline, "appsink must be valid");
		return;
	}

	pipeline->bus = gst_pipeline_get_bus(pipeline->gst_pipeline);
	gst_bus_add_signal_watch(pipeline->bus);
	gst_bus_enable_sync_message_emission(pipeline->bus);
	if (g_signal_connect(pipeline->bus, "message", G_CALLBACK(on_bus_message),
			     pipeline) == 0)
	{
		errorf(pipeline, "Couldn't connect to pipeline messages");
		return;
	}
	if (g_signal_connect(pipeline->bus, "sync-message::element",
			     G_CALLBACK(on_bus_sync_message), pipeline) == 0)
	{
		errorf(pipeline, "Couldn't connect to pipeline sync messages");
		return;
	}
}

///////////////
// Public API
///////////////

void xg_init(int *argc, char **argv[])
{
	gtk_init(argc, argv);
	gst_init(argc, argv);
}

xg_pipeline *xg_create_video_overlay_pipeline(const char *window_title, 
						const char *device, bool gui)
{
	xg_pipeline *pipeline = xg_create_base_pipeline(window_title);
	if (pipeline->error_occurred)
	{
		return NULL;
	}

	if (gui)
		build_window(pipeline);

	build_video_overlay_pipeline(pipeline, device);
	if (pipeline->error_occurred)
	{
		xg_pipeline_free(pipeline);
		return NULL;
	}

	xg_base_pipeline_init(pipeline);
	if (pipeline->error_occurred)
	{
		xg_pipeline_free(pipeline);
		return NULL;
	}
	else
	{
		return pipeline;
	}
}

void xg_pipeline_start(xg_pipeline *pipeline)
{
	gtk_widget_show_all(GTK_WIDGET(pipeline->window));

	if (gst_element_set_state(GST_ELEMENT(pipeline->gst_pipeline),
				  GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
	{
		errorf(pipeline,
		       "Couldn't change state of pipeline to playing\n"
		       "(Do you have a webcam plugged in?)");
		// TODO: Any way to get more detailed diagnostic info? This particular
		// failure doesn't generate any bus messages, for some reason.
		return;
	}
	pipeline->running = true;
}

void xg_pipeline_toggle_pause(xg_pipeline *pipeline)
{
	GstState cur_state;
	gst_element_get_state(GST_ELEMENT(pipeline->gst_pipeline), &cur_state, NULL,
			      GST_SECOND);
	if (cur_state == GST_STATE_PLAYING)
	{
		if (gst_element_set_state(GST_ELEMENT(pipeline->gst_pipeline),
					  GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
		{
			errorf(pipeline, "Couldn't change state of pipeline to playing");
		}
	}
	else if (cur_state == GST_STATE_PAUSED)
	{
		if (gst_element_set_state(GST_ELEMENT(pipeline->gst_pipeline),
					  GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
		{
			errorf(pipeline, "Couldn't change state of pipeline to paused");
		}
	}
}

bool xg_pipeline_running(xg_pipeline *pipeline) { return pipeline->running; }

xg_frame *xg_pipeline_get_frame(xg_pipeline *pipeline)
{
	if (!pipeline->running)
	{
		errorf(pipeline, "Can't get frame from stopped pipeline");
		return NULL;
	}

	while (gtk_events_pending())
	{
		gtk_main_iteration_do(false);
	}

	GstState cur_state;
	gst_element_get_state(GST_ELEMENT(pipeline->gst_pipeline), &cur_state, NULL,
			      GST_SECOND);
	GstSample *gst_sample = NULL;
	if (cur_state == GST_STATE_PLAYING)
	{
		acquire_mutex_or_die(&pipeline->gst_pipeline_lock);
		gst_sample = gst_app_sink_pull_sample(GST_APP_SINK(pipeline->appsink));
		release_mutex_or_die(&pipeline->gst_pipeline_lock);
	}
	else if (cur_state == GST_STATE_PAUSED)
	{
		acquire_mutex_or_die(&pipeline->gst_pipeline_lock);
		gst_sample = gst_app_sink_pull_preroll(GST_APP_SINK(pipeline->appsink));
		release_mutex_or_die(&pipeline->gst_pipeline_lock);
	}
	else
	{
		errorf(pipeline, "Video pipeline is not playing; no frame to return");
		return NULL;
	}

	GstBuffer *data = gst_sample_get_buffer(gst_sample);
	GstStructure *caps_struct =
	    gst_caps_get_structure(gst_sample_get_caps(gst_sample), 0);
	const char *frame_format =
	    strdup(gst_structure_get_string(caps_struct, "format"));
	int32_t frame_width, frame_height;
	gst_structure_get_int(caps_struct, "width", &frame_width);
	gst_structure_get_int(caps_struct, "height", &frame_height);
	gpointer image_data = NULL;
	gsize image_data_size = 0;
	gst_buffer_extract_dup(data, 0, gst_buffer_get_size(data), &image_data,
			       &image_data_size);

	gst_sample_unref(gst_sample);

	xg_frame *result = calloc(1, sizeof(xg_frame));
	if (result == NULL)
	{
		errorf(pipeline, "Failed to allocate memory for frame metadata");
		g_free(image_data);
		return NULL;
	}
	result->format = frame_format;
	result->width = frame_width;
	result->height = frame_height;
	result->data = image_data;
	return result;
}

void xg_pipeline_clear_overlays(xg_pipeline *pipeline)
{
	acquire_mutex_or_die(&pipeline->overlay_lock);
	// free all overlays in the list
	xg_overlay *item = pipeline->overlay_list;
	while (item != NULL)
	{
		xg_overlay *next = item->next;
		xg_overlay_free(item);
		item = next;
	}
	pipeline->overlay_list = NULL;
	release_mutex_or_die(&pipeline->overlay_lock);
}

void xg_pipeline_add_overlay(xg_pipeline *pipeline, xg_overlay *overlay)
{
	if (overlay == NULL)
	{
		return;
	}

	if (overlay->owned_by_pipeline)
	{
		errorf(pipeline, "Overlay already owned by another pipeline");
		return;
	}
	overlay->owned_by_pipeline = true;

	if (pipeline->overlay_list == NULL)
	{
		pipeline->overlay_list = overlay;
		overlay->next = NULL;
		return;
	}

	// Traverse list to end, checking if overlay is already in the list to avoid
	// cycles. This makes it linear in the number of overlays instead of constant
	// time, but there probably aren't very many, so this is acceptable.
	xg_overlay *item = pipeline->overlay_list;
	while (item->next != NULL)
	{
		if (overlay == item)
		{
			return;
		}
		item = item->next;
	}
	item->next = overlay;
	overlay->next = NULL;
}

void xg_pipeline_stop(xg_pipeline *pipeline)
{
	acquire_mutex_or_die(&pipeline->gst_pipeline_lock);
	pipeline->running = false;
	gst_element_set_state(GST_ELEMENT(pipeline->gst_pipeline), GST_STATE_NULL);
	gtk_widget_hide(GTK_WIDGET(pipeline->window));
	release_mutex_or_die(&pipeline->gst_pipeline_lock);
}

void xg_pipeline_free(xg_pipeline *pipeline)
{
	if (pipeline == NULL)
	{
		return;
	}
	if (pipeline->bus)
	{
		g_signal_handlers_disconnect_by_func(pipeline->bus, on_bus_message,
						     pipeline);
		g_signal_handlers_disconnect_by_func(pipeline->bus, on_bus_sync_message,
						     pipeline);
	}
	if (pipeline->window)
	{
		g_signal_handlers_disconnect_by_func(pipeline->window, on_key_press_event,
						     pipeline);
		g_signal_handlers_disconnect_by_func(pipeline->window, on_destroy_event,
						     pipeline);
	}
	gst_object_unref(pipeline->gst_pipeline);
	gtk_widget_destroy(GTK_WIDGET(pipeline->window));
	free(pipeline);
}

void xg_frame_free(xg_frame *frame)
{
	if (frame == NULL)
	{
		return;
	}
	free((char *)frame->format);
	g_free(frame->data);
	free(frame);
}
