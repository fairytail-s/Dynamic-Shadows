# Dynamic Shadows

Dynamic Shadows is an open-source native mod for Minecraft Bedrock on Android, made for [LeviLauncher](https://github.com/LiteLDev/LeviLaunchroid).

## Features

- Dynamic sun and moon shadows
- Performance, Balanced, High, and Ultra presets
- Adjustable darkness, softness, distance, and shadow color
- Hand and held-item shading
- Shadows fade out next to torches, glowstone and other light sources ("Fade near light sources", needs GL_EXT_shader_framebuffer_fetch)
- Categorized mod-menu settings with persistent configuration

## Requirements

- Android 9 or newer and a 64-bit ARM device (`arm64-v8a`)
- [LeviLauncher](https://github.com/LiteLDev/LeviLaunchroid) to load the mod
- Minecraft Bedrock **26.52**

## Installation

1. Install LeviLauncher.
2. Import `DynamicShadows.levipack` through the launcher's mod manager and enable it.
3. Launch Minecraft through LeviLauncher.

## Build

Requires Android NDK r28c, Python 3, and either xmake or CMake 3.22 or newer.

### xmake

```sh
xmake f -y -p android -a arm64-v8a -m release --ndk=/path/to/android-ndk-r28c --ndk_sdkver=28
xmake -y
```

### CMake

```sh
cmake -S . -B build-cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/android-ndk-r28c/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --parallel
```

Both builds produce `DynamicShadows.so` and `DynamicShadows.levipack`.

## Light source fade

The mod rewrites the game's block and entity fragment shaders when they are compiled (`src/ShaderPatch.cpp`, glShaderSource hook) so that
opaque pixels carry `1 - block light` in the scene's alpha channel. The shadow pass reads that alpha with
`GL_EXT_shader_framebuffer_fetch` (no extra copy of the scene) and weakens shadows where block light is high. Afterwards alpha is set back to 1.
Draws that have alpha writes switched off by the game get them switched on for that draw only.
