# Copyright (c) 2019 Xnor.ai, Inc.
#
# Change me!
# For the classification samples, any of the models in the "lib" folder with
# "classifier" in the name should work! For the detection samples, any of the
# models in the "lib" folder with "detector" in the name should work.
#
# Be sure to run "make clean" after changing the variable!
#MODEL ?= person-pet-vehicle-detector
MODEL ?= 

SDK_ROOT := .
INCLUDES := -I$(SDK_ROOT)/include
LIBS := -L$(SDK_ROOT)/lib/$(ARCH)/$(MODEL)
CFLAGS += -Wall $(INCLUDES) -g -O3
LINKFLAGS += $(LIBS) -lxnornet -Wl,-rpath '-Wl,$$ORIGIN' -lcairo -lwayland-server -lwayland-client -lwayland-cursor -lwayland-egl

# The GStreamer samples require some headers and system libraries to link with.
# We use the `pkg_config` tool to automatically select the right include paths,
# compile flags, and link options for the versions of the libraries that you
# have installed.
# If you don't have these libraries installed, you can install them through apt
# on Ubuntu from the following packages:
#   libgtk-3-dev libgstreamer-1.0-dev libgstreamer-plugins-base1.0-dev
#   libgstreamer-plugins-good1.0-dev
PKG_CONFIG_LIBS := fuse gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0 gtk+-3.0 wayland-client wayland-protocols
XGFLAGS := $(shell pkg-config --cflags $(PKG_CONFIG_LIBS))
XGLIBS := $(shell pkg-config --libs $(PKG_CONFIG_LIBS))

.PHONY: all clean

all: build/object_detector \
	build/classify_image_file \
	build/detect_and_print_objects_in_image \
	build/json_dump_objects_in_image \
	build/model_benchmark \
	build/segmentation_mask_of_image_file_to_file \
	build/gstreamer_live_overlay_object_detector \
	build/gstreamer_live_overlay_scene_classifier \
	build/videotest \
	build/intercomm \
	build/gstreamer_toradex_faces_sides

clean:
	rm -rf build/

# Model lib
build/libxnornet.so : $(SDK_ROOT)/lib/$(ARCH)/libxnornet.so
	mkdir -p build
	cp -f $< $@

# Common utilities
build/common_util/tmp_intercomm.o : common_util/tmp_intercomm.h
build/common_util/file.o : common_util/file.h
build/common_util/colors.o : common_util/colors.h
build/common_util/overlays.o : common_util/overlays.h common_util/colors.h
build/common_util/viewporter-client-protocol.o : common_util/viewporter-client-protocol.h
build/common_util/overlays.o build/common_util/gstreamer_video_pipeline.o : \
	CFLAGS += $(XGFLAGS)
build/common_util/%.o : common_util/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Sample binaries
build/% : %.c build/common_util/file.o \
	build/common_util/viewporter-client-protocol.o | build/libxnornet.so
	$(CC) $(CFLAGS) $(XGFLAGS) $^ $(XGLIBS) $(LINKFLAGS) -o $@

build/gstreamer_% : gstreamer_%.c \
	build/common_util/tmp_intercomm.o \
	build/common_util/colors.o \
	build/common_util/gstreamer_video_pipeline.o \
	build/common_util/overlays.o | \
	build/libxnornet.so
	$(CC) $(CFLAGS) $(XGFLAGS) $^ $(XGLIBS) $(LINKFLAGS) -o $@
