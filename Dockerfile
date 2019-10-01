FROM arm64v8/debian:buster-slim AS build

# install dev deps
RUN apt-get -y update && apt-get install -y \
	libc6 \
	libc6-dev \
	binutils \
	make \
	cmake \
	clang \
	gcc g++ \
	git \
	python \
	pkg-config \
	libx11-dev libgl1-mesa-dev \
	libjpeg-dev libpng-dev \
	libwayland-client0 libwayland-dev \
	libgstreamer1.0-0 \
	libgtk-3-0 \
	libgtk-3-dev \
	libgstreamer1.0-dev \
	gstreamer1.0-x \
	gstreamer1.0-tools \
	gstreamer1.0-plugins-good \
	gstreamer1.0-plugins-base \
	gstreamer1.0-plugins-bad \
	gstreamer1.0-alsa \
	libgstreamer-plugins-base1.0-dev \
	libgstreamer-plugins-bad1.0-dev \
	libgirepository1.0-dev \
	fuse \
	libfuse2 \
	libfuse-dev \
	&& apt-get clean && apt-get autoremove && rm -rf /var/lib/apt/lists/*

# copy project source code
COPY /project /project

# compile
WORKDIR /project
RUN MACHINE_ARCH=`arch` && \
    make clean && \
    make ARCH=$MACHINE_ARCH

# deploy steps
FROM arm64v8/debian:buster-slim AS deploy

# install ai2go deps
RUN apt-get -y update && apt-get install -y \
	udev \
	kmod \
	libglx-mesa0 \
	libegl1 \
	libwayland-client0 \
	libwayland-egl1 \
	libgstreamer1.0-0 \
	libgtk-3-0 \
	gstreamer1.0-plugins-good \
	gstreamer1.0-plugins-bad \
	gstreamer1.0-plugins-base \
	libatomic1 \
	fuse \
	libfuse2 \
	shared-mime-info \
	&& apt-get clean && apt-get autoremove && rm -rf /var/lib/apt/lists/*

# copy binary
COPY --from=build /project/gstreamer_live_overlay_object_detector /project/gstreamer_live_overlay_object_detector

# copy marketing image
COPY project/demoXnorTorizon.png /project/demoXnorTorizon.png

# copy ui
COPY project/window.ui /project/window.ui

# entry point args
ENV APP="/ai2go/gstreamer_live_overlay_object_detector"
ENV APPARGS=""

# cmd as argument
CMD [ "/dev/video4", "nogui" ]
