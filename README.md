# XNOR.ai #
<!-- Change the headline to the Partner's Name -->

[XNOR.ai](https://www.toradex.com/pt-br/support/partner-network/services/100008/xnorai) develop state-of-the-art on-device AI solutions that enable business to
real-time decisions, deliver more efficient experiences to customers. 

<iframe width="500" height="315" src="https://www.youtube.com/embed/soriyOcetQk" frameborder="0" allow="accelerometer; autoplay; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>

<!--Write some words about the Partner Company, what do they do and link to their website -->

## torizonextras/arm64v8-ai2go XNOR.ai AI2GO Object Detector Demo ##
<!-- Change the header to whatever the Image is about, e.g. Android, Qt Framework, etc -->

[torizonextras/arm64v8-ai2go](https://cloud.docker.com/u/torizonextras/repository/docker/torizonextras/arm64v8-ai2go "torizonextras/arm64v8-ai2go XNOR.ai AI2GO Object Detector Demo") Partner Demo Image comes with the Object Detector demo that use the [AI2GO](https://ai2go.xnor.ai/toradex/_) Detection models.

<!-- What does the image do? Write a few words here about that -->

## Supported Modules ##

The following Computer on Modules are supported:

- [Apalis iMX8](https://www.toradex.com/pt-br/computer-on-modules/apalis-arm-family/nxp-imx-8)

<!-- List other support accessories, e.g. -->

## Supported Cameras ##

- Any USB WebCam with YUYV format
- [CSI Camera](https://www.toradex.com/pt-br/accessories/csi-camera-module-5mp-ov5640) 

## Supported Displays ##

- HDMI (1920x1080)

<!-- The following is standard text, do not modify it and make sure the link to the display section works well before publishing the article -->

While you can use a wide variety of displays and monitors, additional configuration may be required for a specific setup.

A [section in the end of this guide](#Display_and_Touch_Screen "Display Configuration") provide instructions about display and touch screen configuration.

# How to Get Started #

This section provides instructions for you to quickly get started with torizonextras/arm64v8-ai2go.

<!-- Write a step by step guide on how to install the image and do something cool with it.
If there are different instructions for different modules, use tabs, otherwise just
write the regular markdown. On such case, try to use a single tabs structure throughout the article and make as most of the content as possible equal among tabs. -->

For this demo you will need a camera for image capture and an HDMI screen.

### Demo Architecture ###

This demonstration requires two containers for its operation. The container with the AI2GO object detection application and the container with a graphic composer, as the AI2GO application will display the captured video with bound boxes to highlight the detected objects:

![Torizon AI2GO Container Demo Architecture](https://docs1.toradex.com/106704-demoai2goarch.png?w=500)

Another important note is that in order to facilitate the detection model change, AI2GO has several models that can be downloaded, libxnornet.so, detection model file, will have to be stored on the Torizon Core host outside the container. The AI2GO application container will have the model file path shared between the host.

### Docker Compose ###

For organization and correct start order of the containers we recommend using Docker Compose:

<script src="https://gist.github.com/microhobby/9ae325b6360a1bbf10dd7312e4360817.js"></script>

Create or copy the file above with the same content and the name "docker-compose.yaml" on the 
board running Torizon.

## Using The torizonextras/arm64v8-ai2go XNOR.ai AI2GO Object Detector Demo ##

<!-- If content is module-specific, move this section to the tabs below -->

[TABS]
[TAB-TITLE] Apalis iMX8 [/TAB-TITLE]
[TAB-CONTENT]

## Pull ##

To pull the container images use the below command on the root folder where the "docker-compose.yml" file is located on the board:

```
# docker-compose pull
```

## Shared Model Library ##

Download any detection model from the [AI2GO](https://ai2go.xnor.ai/toradex/_) for Toradex Boards and copy to the your board:

```
$ scp libxnornet.so torizon@your_ip:/home/torizon
```

{.warning} make sure you copy the model libxnornet.so file to the /home/torizon folder.

{.warning} change the "your_ip" on the command above for the IP address from your Apalis iMX8 board.

## Run the Demo ##

To run the demo use the below command on the root folder where the "docker-compose.yml" file is located on the board:

```
# docker-compose up
```

You should see the following screen with real-time video captured from the camera with the bound boxes:

![XNOR.ai AI2GO Demo running](https://docs.toradex.com/106705-xnorai2godemorunning.png?w=500)

In the screenshot above we are using the face detection model.

[/TAB-CONTENT]
[/TABS]

## Next Steps ##

For more information about the Docker image, Dockerfile, and the source code for this demo see our repo:
https://gitlab.int.toradex.com/ma/demos/ai2go-imx8

<!-- How to get Started should always end with Next Steps, this can point to the partner's documentation or say
some cool stuff about the image, etc -->

# Display and Touchscreen Configuration #

<!-- Always use the shared article below. If needed add this specific demo notes before or after it. -->

<!-- If there is no touch-screen use the following note -->
{.note} This demo application does not have touch screen interactivity.

ARTICLE_CONTENT [[102904]]

# Release Notes #

<!-- This is mandatory. If there are no notes, just state something such as "initial release" or "new release"

This release notes are strictly related to the test of the <demo image name> images in Toradex hardware. They are not related to the <demo technology> releases. -->

## Initial Release ##
- Using arm32v7 lib and container
- Using pixbuff mode for Weston
