# Mocktail VR (experimental)

Build this checkout, start WiVRn or SteamVR/ALVR, then connect your headset before launching:

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMOCKTAIL_ENABLE_VR=ON
cmake --build build -j4
./build/mocktail --vr
```

## Automatic runtime selection

Normally just run `./build/mocktail --vr`. The default `auto` mode detects a
running SteamVR server (also used by ALVR) or a listening WiVRn IPC socket for
the current desktop user. A stale socket file does not count as a running
server. If exactly one server is detected, its runtime takes priority over an
old registration for the other server. If both or neither are detected, the
registered OpenXR runtime has priority.

SteamVR is also found through `openvr/openvrpaths.vrpath`, standard Steam
locations, and the running server's executable path. WiVRn development builds
are discovered near the running server executable; a matching registered
WiVRn manifest is preferred, including a Flatpak registration. The selected
path and the reason for selecting it are printed at startup.

Explicit `XR_RUNTIME_JSON` and `MOCKTAIL_VR_RUNTIME` overrides remain respected.
To return to automatic selection after manually choosing a runtime:

```sh
unset XR_RUNTIME_JSON MOCKTAIL_VR_RUNTIME
./build/mocktail --vr
```

Detection does not start servers, connect a headset, or modify system runtime
registration. It observes the current process/user namespace; container and
host runtimes still need correctly shared libraries and IPC access.

## WiVRn

Start WiVRn and connect its headset client first. Run WiVRn and Mocktail as the
same desktop user, with the same runtime environment; do not start Mocktail
from a root shell while WiVRn runs in your user session. For containers/chroots,
the host runtime library and IPC socket must both be accessible inside.

```sh
MOCKTAIL_VR_RUNTIME=wivrn ./build/mocktail --vr
```

If WiVRn disconnects and exits, reconnect/restart it before retrying Mocktail.
`Connection refused` on `wivrn/comp_ipc` means the selected server is not
accepting the connection. Installing `dbus-launch` alone does not fix that.

## ALVR through SteamVR

Install SteamVR and ALVR, register the ALVR driver using its dashboard, then
connect the headset and confirm SteamVR sees it. Follow the upstream
[ALVR Linux instructions](https://github.com/alvr-org/ALVR/wiki/Linux-Troubleshooting),
including the SteamVR launch-options workaround when required.

```sh
env -u XR_RUNTIME_JSON MOCKTAIL_VR_RUNTIME=alvr \
  MOCKTAIL_GRAPHICS_BACKEND=direct-vulkan ./build/mocktail --vr
```

The command clears an inherited WiVRn manifest override. `steamvr` is an alias
for `alvr`. The selection uses SteamVR's OpenXR runtime;
ALVR handles the streaming. If automatic discovery cannot find a custom
installation, specify its actual manifest:

```sh
XR_RUNTIME_JSON="/path/to/SteamVR/steamxr_linux64.json" ./build/mocktail --vr
```

`XR_RUNTIME_JSON` always wins. `MOCKTAIL_VR_RUNTIME=system` delegates selection
to the OpenXR loader. The default `auto` uses the server detection described above, then registered
and installed runtimes. Selection affects only this process.

## OpenGL ES

The `opengl`/`system` renderer uses the guest's real EGL/OpenGL ES context:

```sh
MOCKTAIL_GRAPHICS_BACKEND=opengl ./build/mocktail --vr
```

The runtime must provide `XR_KHR_opengl_es_enable` and `XR_MNDX_egl_enable`, and
support sharing images with the guest EGL device. Both eye copies stay on the
GPU; no Python viewer or CPU readback is required. Camera tracking uses the
same OpenXR frame/pose pipeline as Vulkan. If the runtime cannot support this
EGL binding, use `MOCKTAIL_GRAPHICS_BACKEND=direct-vulkan`; the ALVR command
above deliberately selects Vulkan.

## Current validation and limitations

- Native VR camera/eye hooks are verified for Roblox build **2998**, Build ID
  `ade08266c67aee88ec9c1d00902150e1684dad3a`. Approval of a newer payload for
  ordinary rendering does **not** establish VR compatibility. Do not bypass
  the Build-ID checks to run the old VR hooks on build 3092.
- GLES framebuffer copying, distinct eyes, state restoration, resource
  invalidation, and an EGL OpenXR session with session-loss recovery have been
  tested. A local unauthenticated Roblox run on hardware GLES submitted over
  240 projection frames through a simulated Monado headset.
- This host's software GLES/Vulkan combination could create XR texture names
  but not usable framebuffer attachments. The copy failure is reported and
  XR output stops; this is not counted as a successful rendering test.
- The local Monado test runtime reports `u_graphics_buffer_unref: Bad file
  descriptor (fd: 0)` during GLES teardown, including successful hardware
  runs. This remains an integration caveat; clean headset teardown is not
  claimed from those simulator runs. During auto-selection validation, one
  run also failed a later EGL session recreation with
  `vkCreateFence: VK_ERROR_OUT_OF_HOST_MEMORY`; a fresh repeat passed.
  Runtime selection and initial frame submission succeeded in both runs.
- Real headset acceptance through WiVRn and ALVR/SteamVR is still pending.
  Controller input and canted eye orientations are not implemented.
- Use `./build/mocktail --no-vr` for ordinary rendering. No helper script is
  needed for normal VR startup.

The GLES OpenXR integration test is opt-in and needs an already connected
runtime (or a separately prepared simulator). Run from a graphical desktop:

```sh
MOCKTAIL_TEST_XR_GLES=1 ./build/tests/gles_vr_transport_test \
  --gtest_filter='*OpenXrEglSession*'
```
