# linux_arm_dispaly HMI

This project builds the Linux ARM HMI runtime for direct-connected displays. It uses `LvglLuaBindingHmi`, LVGL DRM/KMS display output, and optional evdev input.

Build on the Linux ARM target or in a matching cross-build environment:

```sh
qmake linux_arm_dispaly/linux_arm_dispaly.pro
make -j$(nproc)
```

Run a designer-generated Lua project:

```sh
./hmi /path/to/project/project.lua --drm /dev/dri/card0 --connector -1 --touch /dev/input/event1
```

Options:

- `--drm <path>`: DRM card path, default `/dev/dri/card0`.
- `--connector <id>`: DRM connector id, default `-1` for auto selection.
- `--touch <path>` or `--pointer <path>`: evdev pointer/touch input path.
- `--keypad <path>`: evdev keypad/keyboard input path.
- `--log <path>`: append stdout and stderr to a log file.

The default Windows top-level project does not include this HMI project because it is Linux-only.