# SWL (Soilleir WL):

## Building:
To build please install all the needed deps and then run `make` to build the code

## Running:
### This software is in early development!
This software is in early development and not really supposed to be run currently. However it technically can be run by building the `src/backend.c` file. And setting the `SWL_DRM_OVERRIDE` and the `SWL_KEYBOARD` environment variables to be the file drm card and keyboard event file you wish to use.

However you will be unable to spawn a client without either blindly typing into the TTY with needed wayland display variables set or sleeping a program to start on the server start. The TTY is left in text mode purely so if some goes wrong you should be able to hit `CTRL+C` and that will kill the app. You can then swap to another TTY which *should* restore your graphics output.

### Problems in the current build
- TTY is left in text mode
- Swapping TTYs while running will cause an error as no TTY swap code is present.
- Only one output and GPU is supported
- Multiple clients don't work
- Everything is software rendered.
- more...

### If you do test:
Please do report any issues you find especially if you have access to hardware I don't. i.e. RPIs, VisionFives, Intel, more AMDGPUs and more Nouveau/Nvidia Prop driver GPUs.

## Deps:
### Required:
- libwayland-server
- libxkbcommon
- libdrm

### Optional:

