# SWL (Soilleir WL):
A Compositor Library written from Scratch

## Building:
To build please install all the needed deps and then run `make` to build the code

## Running:
### This software is in early development!
This software is in early development and not really supposed to be run currently. However it technically can be run by building using `make` file. And setting the `SWL_DRM_DEVICE` environment variable to be the file drm card if it's not set it defaults `/dev/dri/card0`.

Keybinds:
All keybindings are combined with the modifier `CTRL`

- Enter/Return (Spawn a havoc terminal)
- F[1-12] (Swap to TTY #)
- Escape (Exit)
- Tab (Swap active client)
- q (Quit the currently focused client)

Mouse:
- Resize Client Mouse + Alt
- Move Client Mouse + Ctrl


### Problems in the current build
- Only one GPU is supported
- more...

### If you do test:
Please do report any issues you find especially if you have access to hardware I don't. i.e. RPIs, VisionFives, Intel, more AMDGPUs and more Nouveau/Nvidia Prop driver GPUs.

## Filling out an issue report:
if you fill out an issue on github please use the following format or atleast something close to it depending on the issue.
Title: <Issue Description> On <Hardware>.

## Deps:
### Required:
- libwayland-server
- libxkbcommon
- libdrm
- libseat
- libinput
- libudev/libudev-zero(Or just any compat lib)
- egl & gles
- libxcb
- libxcb-dri3
- libxcb-render
- libxcb-renderutil
- libxcb-present

### Optional:


## Debugging:
If you wish to get a debug log pass `--log-file FileName` to the ./minisoilleir Test program
