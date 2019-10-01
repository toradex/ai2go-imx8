#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <wayland-client.h>
#include "common_util/viewporter-client-protocol.h"

int done = 0;
struct wl_compositor *compositor;
struct wl_shell *shell;
struct wl_shm *shm;
struct wp_viewporter *viewporter;
struct wp_viewport *viewport;
struct wl_surface *surface;
GstElement *videosink;

void sig_handler(int signum, siginfo_t *siginfo, void *ctx)
{
    done = 1;
}

static void cb_global_registry_handler(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
    //fprintf(stderr, "Got a registry event for %s id %d\n", interface, id);

    if (strcmp(interface, "wl_compositor") == 0)
        compositor = (struct wl_compositor *) wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    else if (strcmp(interface, "wl_shell") == 0)
        shell = (struct wl_shell *) wl_registry_bind(registry, id, &wl_shell_interface, 1);
    else if (strcmp(interface, "wl_shm") == 0)
        shm = (struct wl_shm *) wl_registry_bind(registry, id, &wl_shm_interface, 1);
    else if (strcmp(interface, "wp_viewporter") == 0)
        viewporter = (struct wp_viewporter *) wl_registry_bind(registry, id, &wp_viewporter_interface, 1);
}

static void cb_global_registry_remover(void *data, struct wl_registry *registry, uint32_t id)
{
}

static const struct wl_registry_listener registry_listener =
{
    cb_global_registry_handler,
    cb_global_registry_remover
};

static void cb_shell_surface_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

static void cb_shell_surface_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height)
{
    wp_viewport_set_destination(viewport, width, height);
    wl_surface_commit(surface);
    gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(videosink), 10, 10, width-20, height-20);
}

static void cb_shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener =
{
    cb_shell_surface_ping,
    cb_shell_surface_configure,
    cb_shell_surface_popup_done
};

int main(int argc, char *argv[])
{
    struct sigaction sa;
    sa.sa_sigaction = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGINT, &sa, NULL);

    fprintf(stderr, "init\n");
    gst_init(&argc, &argv);
    GError *e = NULL;
    GstElement *pipeline = gst_parse_launch("v4l2src device=/dev/video1 ! autovideoconvert ! waylandsink name=sink window-width=320 window-height=240", &e);

    fprintf(stderr, "create display\n");
    struct wl_display *display = wl_display_connect(NULL);
    fprintf(stderr, "display=%p\n", display);
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);
    fprintf(stderr, "compositor=%p\n", compositor);
    fprintf(stderr, "shell=%p\n", shell);
    fprintf(stderr, "shm=%p\n", shm);
    fprintf(stderr, "viewporter=%p\n", viewporter);

    fprintf(stderr, "create surface\n");
    surface = wl_compositor_create_surface(compositor);
    fprintf(stderr, "surface=%p\n", surface);
    struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(shell, surface);
    fprintf(stderr, "shell_surface=%p\n", shell_surface);
    wl_shell_surface_set_toplevel(shell_surface);
    wl_shell_surface_add_listener(shell_surface, &shell_surface_listener, NULL);
    wl_surface_commit(surface);

    fprintf(stderr, "create buffer\n");
    int buffer_width = 1;
    int buffer_height = 1;
    int buffer_stride = buffer_width * sizeof(uint32_t);
    int buffer_size = buffer_stride * buffer_height;
    fprintf(stderr, "buffer_size=%d\n", buffer_size);
    char filename[] = "/tmp/gst-wayland-test-XXXXXX";
    int fd = mkostemp(filename, O_CLOEXEC);
    fprintf(stderr, "fd=%d, filename=%s\n", fd, filename);
    ftruncate(fd, buffer_size);
    uint32_t *data = (uint32_t *) mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    fprintf(stderr, "data=%p\n", data);
    uint32_t *p = data;
    for(int y=0; y<buffer_height; y++)
        for(int x=0; x<buffer_width; x++)
            *p++ = 0xFF00FF00;
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, (int32_t) fd, buffer_size);
    fprintf(stderr, "pool=%p\n", pool);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, buffer_width, buffer_height, buffer_stride, WL_SHM_FORMAT_ARGB8888);
    fprintf(stderr, "buffer=%p\n", buffer);
    wl_shm_pool_destroy(pool);
    wl_surface_attach(surface, buffer, 0, 0);

    fprintf(stderr, "create viewport fake\n");
    int window_width=320;
    int window_height=240;
    viewport = wp_viewporter_get_viewport(viewporter, surface);
    fprintf(stderr, "viewport=%p\n", viewport);
    wp_viewport_set_source(viewport, 0, 0, buffer_width, buffer_height);
    wp_viewport_set_destination(viewport, window_width, window_height);
    wl_surface_commit(surface);

    fprintf(stderr, "set gstreamer overlay\n");
    GstContext *context = gst_context_new("GstWaylandDisplayHandleContextType", TRUE);
    fprintf(stderr, "context=%p\n", context);
    gst_structure_set(gst_context_writable_structure(context), "handle", G_TYPE_POINTER, display, NULL);
    gst_element_set_context(pipeline, context);
    videosink = gst_bin_get_by_name((GstBin *)pipeline, "sink");
    fprintf(stderr, "videosink=%p\n", videosink);
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videosink), (guintptr) surface);
    gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(videosink), 10, 10, window_width-20, window_height-20);
    wl_surface_commit(surface);

    fprintf(stderr, "start pipeline\n");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    while (!done)
    	wl_display_dispatch_pending(display);

    fprintf(stderr, "stop pipeline\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videosink), (guintptr) NULL);
    gst_deinit();

    fprintf(stderr, "cleanup\n");
    wl_surface_destroy(surface);
    munmap(data, buffer_size);
    close(fd);
    wl_display_disconnect(display);

    fprintf(stderr, "done\n");
    return 0;
}