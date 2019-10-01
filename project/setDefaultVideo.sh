#!/bin/bash

# lets check /dev/video
DEVICES="$(ls -1v /dev/video*)"
# split in
DEVICESIN=(${DEVICES//\n/ })

# the default device will be the device that return 0 from the try fmt
DEFAULT="/dev/video0"

# check device that accept my constraints
for i in "${DEVICESIN[@]}"; do
    #echo "Testing $i"
    v4l2-ctl -d $i --try-fmt-video=width=320,height=240,pixelformat=YUYV &> /dev/null
    #echo $?
    if [ 0 -eq $? ]
    then
        DEFAULT=$i
        break
    fi
done

echo "Device :: $DEFAULT"
v4l2-ctl -d $DEFAULT --info

echo "Set the default $DEFAULT as /dev/video0"

if [ -h /dev/video0 ]; then 
    ls -lah /dev/video0
else
    mv /dev/video0 /dev/video0.original
    ln -s $DEFAULT /dev/video0
fi

# copy the bundle from the torizon
rm /project/libxnornet.so
cp /xnor/libxnornet.so /project/libxnornet.so

# and now we run the app
./project/gstreamer_live_overlay_object_detector $@
