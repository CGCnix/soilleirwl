# SWL (Soilleir WL):

## Building:
To build please install all the needed deps and then run `make` to build the code

## Running:
### This software is in early development!
This software is in early development and not really supposed to be run currently. However it technically can be run by building using `make` file. And setting the `SWL_DRM_DEVICE` environment variable to be the file drm card if it's not set it defaults `/dev/dri/card0`.

Keybinds:
All keybindings are combined with the modifiers `CTRL+ALT`

- Enter/Return (Spawn a havoc terminal)
- F[1-12] (Swap to TTY #)
- Escape (Exit)
- Tab (Swap active client)
- Arrow keys move client 10px that direction (NOTE: this code assumes 1920x1080 size)


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

